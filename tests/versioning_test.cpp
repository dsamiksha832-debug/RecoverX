#include "recoverx/chunkstore.hpp"
#include "recoverx/crc32.hpp"
#include "recoverx/manifest.hpp"
#include "recoverx/registration.hpp"
#include "recoverx/recovery.hpp"
#include "recoverx/verification.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

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

int main() {
    const fs::path root = fs::temp_directory_path() / "recoverx_versioning_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);

    VersionInfo sample;
    sample.versionId = 42;
    sample.originalFileName = "sample.bin";
    sample.originalSize = 3;
    sample.status = VersionStatus::Unverified;
    sample.chunks.push_back(ChunkInfo{std::string(64, 'a'), 0x12345678u, 3});
    const fs::path manifest = root / "roundtrip.rxm";
    CHECK(writeManifest(manifest.string(), sample));
    VersionInfo decoded;
    CHECK(readManifest(manifest.string(), decoded));
    CHECK(decoded.versionId == sample.versionId);
    CHECK(decoded.originalFileName == sample.originalFileName);
    CHECK(decoded.originalSize == sample.originalSize);
    CHECK(decoded.status == sample.status);
    CHECK(decoded.chunks.size() == 1);
    CHECK(decoded.chunks[0].sha256 == sample.chunks[0].sha256);
    CHECK(decoded.chunks[0].crc32 == sample.chunks[0].crc32);
    CHECK(decoded.chunks[0].size == sample.chunks[0].size);

    Bytes damaged = readBytes(manifest);
    damaged[5] ^= 0x01;
    writeBytes(root / "damaged.rxm", damaged);
    VersionInfo unchanged;
    unchanged.versionId = 999;
    CHECK(!readManifest((root / "damaged.rxm").string(), unchanged));
    CHECK(unchanged.versionId == 999); // failed parsing leaves the output untouched

    const fs::path source = root / "source.bin";
    Bytes contents(10000);
    for (std::size_t i = 0; i < contents.size(); ++i) contents[i] = static_cast<std::uint8_t>((i * 29) & 0xff);
    writeBytes(source, contents);
    const fs::path storeRoot = root / "store";
    VersionInfo first;
    CHECK(registerVersion(source.string(), storeRoot.string(), 4096, first));
    CHECK(first.versionId == 1);
    CHECK(first.status == VersionStatus::Unverified);
    CHECK(first.originalSize == contents.size());
    CHECK(first.chunks.size() == 3);
    CHECK(first.chunks[0].size == 4096 && first.chunks[2].size == 1808);
    for (const auto& chunk : first.chunks) {
        CHECK(ChunkStore::isValidContentId(chunk.sha256));
    }
    VersionInfo saved;
    CHECK(readManifest((storeRoot / "manifests" / "version_1.rxm").string(), saved));
    CHECK(saved.originalSize == first.originalSize && saved.chunks.size() == first.chunks.size());

    const fs::path restoredPath = root / "restored.bin";
    CHECK(restoreVersion(first.versionId, storeRoot.string(), restoredPath.string()));
    CHECK(readBytes(restoredPath) == contents);

    ChunkStore verifiedStore(storeRoot.string());
    CHECK(verifiedStore.scan() > 0);
    VersionInfo verified = first;
    CHECK(verifyVersion(verified, verifiedStore));
    CHECK(verified.status == VersionStatus::Valid);

    // A same-size byte change is caught by the content ID and CRC checks.
    const fs::path damagedChunk = verifiedStore.chunkPath(first.chunks[0].sha256);
    Bytes altered = readBytes(damagedChunk);
    altered[0] ^= 0x80;
    writeBytes(damagedChunk, altered);
    VersionInfo corrupt = first;
    CHECK(!verifyVersion(corrupt, verifiedStore));
    CHECK(corrupt.status == VersionStatus::Corrupt);
    const fs::path protectedOutput = root / "protected-output.bin";
    const Bytes sentinel{'k', 'e', 'e', 'p'};
    writeBytes(protectedOutput, sentinel);
    CHECK(!restoreVersion(first.versionId, storeRoot.string(), protectedOutput.string()));
    CHECK(readBytes(protectedOutput) == sentinel);

    const fs::path missingRoot = root / "missing-store";
    VersionInfo missingVersion;
    CHECK(registerVersion(source.string(), missingRoot.string(), 4096, missingVersion));
    ChunkStore missingStore(missingRoot.string());
    CHECK(missingStore.scan() > 0);
    fs::remove(missingStore.chunkPath(missingVersion.chunks[0].sha256));
    CHECK(!verifyVersion(missingVersion, missingStore));
    CHECK(missingVersion.status == VersionStatus::Missing);

    VersionInfo badMetadata = first;
    ++badMetadata.originalSize;
    CHECK(!verifyVersion(badMetadata, verifiedStore));
    CHECK(badMetadata.status == VersionStatus::Corrupt);

    VersionInfo second;
    CHECK(registerVersion(source.string(), storeRoot.string(), 4096, second));
    CHECK(second.versionId == 2);
    CHECK(second.chunks.size() == first.chunks.size());

    VersionInfo untouched;
    untouched.versionId = 77;
    CHECK(!registerVersion(source.string(), (root / "bad-store").string(), 0, untouched));
    CHECK(untouched.versionId == 77);

    // Recovery skips a newer corrupted version and atomically restores an older valid one.
    const fs::path recoverySource = root / "recovery-target.bin";
    const fs::path recoveryStoreRoot = root / "recovery-store";
    Bytes olderContents(7000);
    for (std::size_t i = 0; i < olderContents.size(); ++i) {
        olderContents[i] = static_cast<std::uint8_t>((i * 17 + 3) & 0xff);
    }
    writeBytes(recoverySource, olderContents);
    VersionInfo olderVersion;
    CHECK(registerVersion(recoverySource.string(), recoveryStoreRoot.string(), 4096, olderVersion));

    Bytes newerContents = olderContents;
    newerContents[0] ^= 0x55;
    writeBytes(recoverySource, newerContents);
    VersionInfo newerVersion;
    CHECK(registerVersion(recoverySource.string(), recoveryStoreRoot.string(), 4096, newerVersion));
    CHECK(newerVersion.versionId > olderVersion.versionId);

    ChunkStore recoveryStore(recoveryStoreRoot.string());
    recoveryStore.scan();
    Bytes damagedNewest = readBytes(recoveryStore.chunkPath(newerVersion.chunks[0].sha256));
    damagedNewest[0] ^= 0x01;
    writeBytes(recoveryStore.chunkPath(newerVersion.chunks[0].sha256), damagedNewest);
    CHECK(recoverLatestValidVersion(recoverySource.string(), recoveryStoreRoot.string()));
    CHECK(readBytes(recoverySource) == olderContents);

    const fs::path undoSource = root / "undo-target.bin";
    const fs::path undoStoreRoot = root / "undo-store";
    const Bytes beforeChange{'b', 'e', 'f', 'o', 'r', 'e'};
    const Bytes afterChange{'a', 'f', 't', 'e', 'r'};
    writeBytes(undoSource, beforeChange);
    VersionInfo beforeVersion;
    CHECK(registerVersion(undoSource.string(), undoStoreRoot.string(), 3, beforeVersion));
    writeBytes(undoSource, afterChange);
    VersionInfo afterVersion;
    CHECK(registerVersion(undoSource.string(), undoStoreRoot.string(), 3, afterVersion));
    CHECK(undoLastChange(undoSource.string(), undoStoreRoot.string()));
    CHECK(readBytes(undoSource) == beforeChange);

    const fs::path singleVersionSource = root / "single-version.bin";
    const fs::path singleVersionStore = root / "single-version-store";
    writeBytes(singleVersionSource, afterChange);
    VersionInfo onlyVersion;
    CHECK(registerVersion(singleVersionSource.string(), singleVersionStore.string(), 3, onlyVersion));
    CHECK(!undoLastChange(singleVersionSource.string(), singleVersionStore.string()));
    CHECK(readBytes(singleVersionSource) == afterChange);

    fs::remove_all(root, ec);
    if (failures == 0) {
        std::cout << "versioning tests passed\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
