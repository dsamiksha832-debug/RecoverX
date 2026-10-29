// Tests for the detailed verify/recovery reports, the undo stack, damaged manifests,
// crash safety, large files and the streaming file digest.
#include "recoverx/checksum.hpp"
#include "recoverx/chunkstore.hpp"
#include "recoverx/crc32.hpp"
#include "recoverx/filehash.hpp"
#include "recoverx/manifest.hpp"
#include "recoverx/recovery.hpp"
#include "recoverx/registration.hpp"
#include "recoverx/sha256.hpp"
#include "recoverx/simulator.hpp"
#include "recoverx/verification.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#define RECOVERX_HAVE_RUSAGE 1
#endif

namespace fs = std::filesystem;
using namespace recoverx;
using Bytes = std::vector<std::uint8_t>;

static int failures = 0;
#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "FAIL line " << __LINE__ << ": " #condition "\n"; ++failures; \
} } while (false)

static void writeBytes(const fs::path& path, const Bytes& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

static Bytes readBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return Bytes(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

static Bytes makeData(std::size_t size, unsigned seed) {
    Bytes data(size);
    std::uint32_t x = seed * 2654435761u + 12345u;
    for (std::size_t i = 0; i < size; ++i) {
        x = x * 1664525u + 1013904223u;
        data[i] = static_cast<std::uint8_t>(x >> 24);
    }
    return data;
}

static void flipByte(const fs::path& path, std::size_t offset) {
    Bytes bytes = readBytes(path);
    bytes.at(offset) ^= 0x01;
    writeBytes(path, bytes);
}

static void testFileDigest(const fs::path& root) {
    const std::size_t sizes[] = {0, 1, 7, 4096, 65536, 65537, 200001};
    for (const std::size_t size : sizes) {
        const Bytes data = makeData(size, 9);
        const fs::path path = root / "digest.bin";
        writeBytes(path, data);
        FileDigest digest;
        CHECK(digestFile(path.string(), digest));
        CHECK(digest.size == size);
        CHECK(digest.crc32 == crc32(data));
        CHECK(digest.internetChecksum == internetChecksum(data));
        CHECK(digest.sha256 == sha256Hex(data));
    }
    FileDigest missing;
    CHECK(!digestFile((root / "no_such_file").string(), missing));
}

static void testVerifyReport(const fs::path& root) {
    const fs::path store = root / "verify_store";
    const fs::path source = root / "v.bin";
    writeBytes(source, makeData(10000, 1));
    VersionInfo v1;
    CHECK(registerVersion(source.string(), store.string(), 4096, v1));
    CHECK(v1.chunks.size() == 3);

    ChunkStore chunks(store.string());
    chunks.scan();
    VersionInfo copy = v1;
    VerifyReport report;
    CHECK(verifyVersion(copy, chunks, report));
    CHECK(report.status == VersionStatus::Valid);
    CHECK(report.failures.empty());
    CHECK(report.chunkCount == 3);

    // A flipped bit in chunk 2 is reported for that chunk, caught by CRC-32 and SHA-256.
    flipByte(chunks.chunkPath(v1.chunks[1].sha256), 5);
    copy = v1;
    CHECK(!verifyVersion(copy, chunks, report));
    CHECK(report.status == VersionStatus::Corrupt);
    CHECK(report.failures.size() == 1);
    if (report.failures.size() == 1) {
        CHECK(report.failures[0].index == 1);
        CHECK(report.failures[0].crc32Mismatch);
        CHECK(report.failures[0].sha256Mismatch);
        CHECK(!report.failures[0].missing);
    }

    // A deleted chunk is reported as missing.
    fs::remove(chunks.chunkPath(v1.chunks[2].sha256));
    copy = v1;
    CHECK(!verifyVersion(copy, chunks, report));
    bool sawMissing = false;
    for (const ChunkFailure& f : report.failures) {
        if (f.index == 2 && f.missing) sawMissing = true;
    }
    CHECK(sawMissing);
}

static void testRecoveryFallbackAndUndoStack(const fs::path& root) {
    const fs::path store = root / "recover_store";
    const fs::path source = root / "records.txt";
    const Bytes v1Data = makeData(20000, 11);
    Bytes v2Data = v1Data; v2Data[100] ^= 0x55;
    Bytes v3Data = v2Data; v3Data[9000] ^= 0x55;
    Bytes v4Data = v3Data; v4Data[15000] ^= 0x55;

    VersionInfo info[4];
    const Bytes* contents[4] = {&v1Data, &v2Data, &v3Data, &v4Data};
    for (int i = 0; i < 4; ++i) {
        writeBytes(source, *contents[i]);
        CHECK(registerVersion(source.string(), store.string(), 4096, info[i]));
    }
    CHECK(info[3].versionId == 4);

    // Corrupt V4 with a burst: only a chunk used by V4 alone is touched.
    CHECK(corruptStoredVersion(source.string(), store.string(), 4, "burst"));

    // The user's file is now something else entirely (the "bad save").
    const Bytes badSave = makeData(5000, 77);
    writeBytes(source, badSave);

    RecoveryReport report;
    CHECK(recoverFile(source.string(), store.string(), report));
    CHECK(report.success);
    CHECK(report.restoredVersionId == 3);
    CHECK(report.attempts.size() == 2);
    if (report.attempts.size() == 2) {
        CHECK(report.attempts[0].versionId == 4);
        CHECK(report.attempts[0].status == VersionStatus::Corrupt);
        CHECK(!report.attempts[0].failures.empty());
        CHECK(report.attempts[1].versionId == 3);
        CHECK(report.attempts[1].status == VersionStatus::Valid);
    }
    CHECK(readBytes(source) == v3Data);                  // byte-identical to V3
    CHECK(report.restoredSha256 == sha256Hex(v3Data));   // and proven by SHA-256
    CHECK(report.restoredBytes == v3Data.size());
    CHECK(report.undoSaved);

    // A second restore stacks a second undo entry.
    RecoveryReport second;
    CHECK(recoverFile(source.string(), store.string(), second, 2));  // --to V2
    CHECK(second.success && second.restoredVersionId == 2);
    CHECK(readBytes(source) == v2Data);

    // --to a corrupt version fails and does not fall back or touch the file.
    RecoveryReport target4;
    CHECK(!recoverFile(source.string(), store.string(), target4, 4));
    CHECK(!target4.success && !target4.error.empty());
    CHECK(readBytes(source) == v2Data);

    // --to a version that does not exist.
    RecoveryReport target9;
    CHECK(!recoverFile(source.string(), store.string(), target9, 9));

    // Undo walks back: first to V3's state, then to the bad save, then nothing is left.
    UndoReport undo;
    CHECK(undoRestore(source.string(), store.string(), undo));
    CHECK(readBytes(source) == v3Data);
    CHECK(undo.remaining == 1);
    CHECK(undoRestore((source.parent_path() / "." / source.filename()).string(), store.string(), undo)); // same file, written differently
    CHECK(readBytes(source) == badSave);
    CHECK(undo.remaining == 0);
    CHECK(!undoRestore(source.string(), store.string(), undo));
    CHECK(!undo.error.empty());

    // Every version corrupt: recovery reports failure honestly and leaves the file alone.
    const Bytes keep = readBytes(source);
    for (std::uint64_t id = 1; id <= 3; ++id) {
        ChunkStore chunks(store.string());
        chunks.scan();
        VersionInfo v;
        CHECK(readManifest((store / "manifests" / ("version_" + std::to_string(id) + ".rxm")).string(), v));
        for (const ChunkInfo& c : v.chunks) fs::remove(chunks.chunkPath(c.sha256));
    }
    ChunkStore chunks(store.string());
    chunks.scan();
    VersionInfo v4;
    CHECK(readManifest((store / "manifests" / "version_4.rxm").string(), v4));
    for (const ChunkInfo& c : v4.chunks) fs::remove(chunks.chunkPath(c.sha256));
    RecoveryReport none;
    CHECK(!recoverFile(source.string(), store.string(), none));
    CHECK(none.attempts.size() == 4);
    CHECK(readBytes(source) == keep);
}

static void testDamagedManifest(const fs::path& root) {
    const fs::path store = root / "manifest_store";
    const fs::path source = root / "m.bin";
    const Bytes data1 = makeData(9000, 21);
    Bytes data2 = data1; data2[10] ^= 0x33;
    writeBytes(source, data1);
    VersionInfo v1, v2;
    CHECK(registerVersion(source.string(), store.string(), 4096, v1));
    writeBytes(source, data2);
    CHECK(registerVersion(source.string(), store.string(), 4096, v2));

    // Damage the manifest of V2 only (its chunks stay intact).
    flipByte(store / "manifests" / "version_2.rxm", 20);
    VersionInfo readBack;
    CHECK(!readManifest((store / "manifests" / "version_2.rxm").string(), readBack));

    std::vector<VersionInfo> versions;
    std::vector<std::uint64_t> damaged;
    CHECK(listVersions(source.string(), store.string(), versions, damaged));
    CHECK(versions.size() == 1 && versions[0].versionId == 1);
    CHECK(damaged.size() == 1 && damaged[0] == 2);

    // Recovery reports the damaged manifest and restores V1; it does not blame any chunk.
    writeBytes(source, makeData(100, 5));
    RecoveryReport report;
    CHECK(recoverFile(source.string(), store.string(), report));
    CHECK(report.restoredVersionId == 1);
    CHECK(report.damagedManifests.size() == 1 && report.damagedManifests[0] == 2);
    CHECK(report.attempts.size() == 1);
    CHECK(readBytes(source) == data1);

    // The chunks themselves are all still intact.
    ChunkStore chunks(store.string());
    chunks.scan();
    for (const ChunkInfo& c : v2.chunks) {
        Bytes bytes;
        CHECK(chunks.get(c.sha256, bytes));
        CHECK(sha256Hex(bytes) == c.sha256);
    }
}

static void testCrashBeforeManifest(const fs::path& root) {
    const fs::path store = root / "crash_store";
    const fs::path source = root / "c.bin";
    const Bytes data1 = makeData(12000, 31);
    writeBytes(source, data1);
    VersionInfo v1;
    CHECK(registerVersion(source.string(), store.string(), 4096, v1));

    // Simulate a backup that stopped after writing its chunks but before the manifest:
    // new chunk files exist, a leftover .tmp file exists, and no manifest was published.
    {
        ChunkStore chunks(store.string());
        chunks.scan();
        std::string id;
        CHECK(chunks.put(makeData(4096, 99), id));
        CHECK(chunks.put(makeData(4096, 98), id));
    }
    writeBytes(store / "chunks" / "interrupted.tmp", Bytes{1, 2, 3});

    std::vector<VersionInfo> versions;
    std::vector<std::uint64_t> damaged;
    CHECK(listVersions(source.string(), store.string(), versions, damaged));
    CHECK(versions.size() == 1);            // the half-done version is not listed
    CHECK(damaged.empty());

    ChunkStore chunks(store.string());
    chunks.scan();                          // startup cleanup removes the stale temp file
    CHECK(!fs::exists(store / "chunks" / "interrupted.tmp"));
    VersionInfo copy = v1;
    CHECK(verifyVersion(copy, chunks));     // the store still verifies

    writeBytes(source, makeData(50, 1));
    CHECK(recoverLatestValidVersion(source.string(), store.string()));
    CHECK(readBytes(source) == data1);      // earlier versions still restore

    // A failed backup (unreadable source) publishes nothing.
    VersionInfo failed;
    CHECK(!registerVersion((root / "does_not_exist.bin").string(), store.string(), 4096, failed));
    CHECK(listVersions(source.string(), store.string(), versions, damaged));
    CHECK(versions.size() == 1);
}

static void testLargeFile(const fs::path& root) {
    const fs::path store = root / "large_store";
    const fs::path source = root / "large.bin";
    constexpr std::size_t kTotal = 16u * 1024u * 1024u;
    constexpr std::size_t kChunk = 1024u * 1024u;

    {
        std::ofstream out(source, std::ios::binary | std::ios::trunc);
        for (std::size_t written = 0; written < kTotal; written += kChunk) {
            const Bytes block = makeData(kChunk, static_cast<unsigned>(written / kChunk + 1));
            out.write(reinterpret_cast<const char*>(block.data()), static_cast<std::streamsize>(block.size()));
        }
    }

    VersionInfo version;
    CHECK(registerVersion(source.string(), store.string(), kChunk, version));
    CHECK(version.originalSize == kTotal);
    ChunkStore chunks(store.string());
    chunks.scan();
    CHECK(verifyVersion(version, chunks));

    RecoveryReport report;
    CHECK(restoreVersion(version.versionId, store.string(), (root / "large_out.bin").string(), report));
    FileDigest original, restored;
    CHECK(digestFile(source.string(), original));
    CHECK(digestFile((root / "large_out.bin").string(), restored));
    CHECK(original.sha256 == restored.sha256);
    CHECK(report.restoredSha256 == original.sha256);

#ifdef RECOVERX_HAVE_RUSAGE
    struct rusage usage {};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
#ifdef __APPLE__
        const double peakMb = static_cast<double>(usage.ru_maxrss) / (1024.0 * 1024.0); // bytes
#else
        const double peakMb = static_cast<double>(usage.ru_maxrss) / 1024.0;             // KB
#endif
        std::cout << "large file test: " << kTotal / (1024 * 1024) << " MB file, peak memory "
                  << peakMb << " MB\n";
        CHECK(peakMb < 12.0); // below the 16 MB file size: memory follows the chunk size, not the file
    }
#endif
}

int main() {
    const fs::path root = fs::temp_directory_path() / "recoverx_report_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);

    testFileDigest(root);
    testVerifyReport(root);
    testRecoveryFallbackAndUndoStack(root);
    testDamagedManifest(root);
    testCrashBeforeManifest(root);
    testLargeFile(root);

    fs::remove_all(root, ec);
    if (failures == 0) {
        std::cout << "report tests passed\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
