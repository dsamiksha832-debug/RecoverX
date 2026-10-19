// Tests for the storage module: atomic writes, streaming chunker, content-addressed chunk store.
// Uses its own CHECK macro (not assert) so the checks still run in Release builds.
#include "recoverx/atomicio.hpp"
#include "recoverx/chunker.hpp"
#include "recoverx/chunkstore.hpp"
#include "recoverx/sha256.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace recoverx;
using Bytes = std::vector<std::uint8_t>;

static int g_failures = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::cerr << "FAIL line " << __LINE__ << ": " #cond << "\n";     \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

static Bytes makeBytes(std::size_t n, unsigned seed) {
    Bytes b(n);
    unsigned x = seed * 2654435761u + 1u;
    for (auto& v : b) {
        x = x * 1664525u + 1013904223u;
        v = static_cast<std::uint8_t>(x >> 24);
    }
    return b;
}

static void writeRaw(const fs::path& p, const Bytes& b) {
    fs::create_directories(p.parent_path());
    std::ofstream o(p, std::ios::binary);
    o.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}

static Bytes readRaw(const fs::path& p) {
    std::ifstream i(p, std::ios::binary);
    return Bytes((std::istreambuf_iterator<char>(i)), std::istreambuf_iterator<char>());
}

static bool anyTmpFiles(const fs::path& dir) {
    for (const auto& e : fs::directory_iterator(dir)) {
        if (e.path().extension() == ".tmp") return true;
    }
    return false;
}

static void testAtomicIo(const fs::path& dir) {
    const fs::path f = dir / "sub" / "a.bin"; // parent folder does not exist yet
    const Bytes a = makeBytes(1000, 1);
    CHECK(atomicWriteFile(f.string(), a));
    CHECK(readRaw(f) == a);
    CHECK(!fs::exists(f.string() + ".tmp"));

    const Bytes b = makeBytes(10, 2); // replaces an existing file
    CHECK(atomicWriteFile(f.string(), b));
    CHECK(readRaw(f) == b);

    CHECK(atomicWriteFile((dir / "empty.bin").string(), Bytes{}));
    CHECK(fs::file_size(dir / "empty.bin") == 0);

    CHECK(!atomicWriteFile((f / "x").string(), a)); // parent is a regular file: must fail
    CHECK(readRaw(f) == b);                         // and leave things unchanged

    Bytes out;
    CHECK(readFileBytes(f.string(), out) && out == b);
    CHECK(!readFileBytes((dir / "missing.bin").string(), out));
    CHECK(!readFileBytes(dir.string(), out)); // a directory is not a readable file

    writeRaw(dir / "tmpdir" / "x.tmp", a);
    writeRaw(dir / "tmpdir" / "keep.txt", a);
    CHECK(removeStaleTempFiles((dir / "tmpdir").string()) == 1);
    CHECK(!fs::exists(dir / "tmpdir" / "x.tmp"));
    CHECK(fs::exists(dir / "tmpdir" / "keep.txt"));
    CHECK(removeStaleTempFiles((dir / "no_such_dir").string()) == 0);
}

static void testChunker(const fs::path& dir) {
    const std::size_t cs = 4096;
    const std::size_t sizes[] = {0, 1, 4095, 4096, 4097, 10000};
    for (const std::size_t n : sizes) {
        const Bytes data = makeBytes(n, static_cast<unsigned>(n) + 3u);
        const fs::path p = dir / ("c_" + std::to_string(n) + ".bin");
        writeRaw(p, data);

        ChunkReader reader(p.string(), cs);
        Bytes chunk, joined;
        std::size_t count = 0;
        while (reader.next(chunk)) {
            ++count;
            CHECK(!chunk.empty() && chunk.size() <= cs);
            joined.insert(joined.end(), chunk.begin(), chunk.end());
        }
        CHECK(count == (n + cs - 1) / cs);
        CHECK(joined == data);
        CHECK(reader.bytesRead() == n);
        CHECK(!reader.next(chunk)); // stays at end of file

        const auto all = splitFile(p.string(), cs);
        CHECK(all.size() == count);
    }

    bool threw = false;
    try { ChunkReader r((dir / "c_1.bin").string(), 0); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
    threw = false;
    try { ChunkReader r((dir / "nope.bin").string(), 4096); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);

    // Larger file, read as a stream of 4 KB pieces.
    const Bytes big = makeBytes(8u * 1024u * 1024u + 123u, 9);
    writeRaw(dir / "big.bin", big);
    ChunkReader r((dir / "big.bin").string(), cs);
    Bytes chunk, joined;
    joined.reserve(big.size());
    while (r.next(chunk)) {
        CHECK(chunk.size() <= cs);
        joined.insert(joined.end(), chunk.begin(), chunk.end());
    }
    CHECK(joined == big);
}

static void testStoreBasics(const fs::path& dir) {
    ChunkStore store((dir / "store").string());
    const Bytes c1 = makeBytes(4096, 11);
    std::string id1, id1b;
    bool isNew = false;

    CHECK(store.put(c1, id1, isNew));
    CHECK(isNew);
    CHECK(ChunkStore::isValidContentId(id1));
    CHECK(id1 == sha256Hex(c1));
    CHECK(store.has(id1));
    CHECK(store.chunkCount() == 1);

    CHECK(store.put(c1, id1b, isNew)); // same bytes: deduplicated
    CHECK(!isNew);
    CHECK(id1b == id1);
    CHECK(store.chunkCount() == 1);
    CHECK(store.put(c1, id1b)); // two-argument form

    Bytes back;
    CHECK(store.get(id1, back));
    CHECK(back == c1);

    CHECK(!store.get("../../etc/passwd", back)); // ids that are not 64 lowercase hex are refused
    CHECK(!store.get("", back));
    CHECK(!store.get(std::string(64, 'g'), back));
    CHECK(!store.get(std::string(64, 'A'), back));
    CHECK(!store.has("short"));
    CHECK(!store.get(std::string(64, '0'), back)); // valid id, nothing stored

    const Bytes empty;
    std::string ide;
    CHECK(store.put(empty, ide));
    CHECK(store.get(ide, back) && back.empty());

    CHECK(!anyTmpFiles(dir / "store" / "chunks")); // no temp files left behind
}

static void testDedupRestoreScan(const fs::path& dir) {
    const std::size_t cs = 4096;
    const Bytes v1 = makeBytes(10 * cs, 21);
    Bytes v2 = v1;
    v2[3 * cs + 5] ^= 0xFF; // change one byte in chunk 3
    writeRaw(dir / "v1.bin", v1);
    writeRaw(dir / "v2.bin", v2);

    const fs::path root = dir / "store2";
    ChunkStore s((root).string());
    auto ingest = [&](const fs::path& p) {
        std::vector<std::string> ids;
        ChunkReader r(p.string(), cs);
        Bytes ch;
        while (r.next(ch)) {
            std::string id;
            CHECK(s.put(ch, id));
            ids.push_back(id);
        }
        return ids;
    };

    const auto ids1 = ingest(dir / "v1.bin");
    CHECK(ids1.size() == 10 && s.chunkCount() == 10);
    const auto ids2 = ingest(dir / "v2.bin");
    CHECK(ids2.size() == 10 && s.chunkCount() == 11); // only the changed chunk is new

    CHECK(s.restoreToFile(ids1, (dir / "out1.bin").string()));
    CHECK(readRaw(dir / "out1.bin") == v1);
    CHECK(s.restoreToFile(ids2, (dir / "out2.bin").string()));
    CHECK(readRaw(dir / "out2.bin") == v2);
    // Restore over an existing file.
    const fs::path outExisting = dir / "out_existing.bin";
    writeRaw(outExisting, makeBytes(100, 99)); // old content

    CHECK(s.restoreToFile(ids2, outExisting.string()));
    CHECK(readRaw(outExisting) == v2);
    CHECK(!fs::exists(outExisting.string() + ".tmp"));

    // A missing chunk makes the restore fail cleanly: no output file, no leftover temp file.
    fs::remove(s.chunkPath(ids1[2]));
    CHECK(!s.has(ids1[2]));
    const fs::path out3 = dir / "out3.bin";
    CHECK(!s.restoreToFile(ids1, out3.string()));
    CHECK(!fs::exists(out3));
    CHECK(!fs::exists(out3.string() + ".tmp"));

    // Backing the file up again rewrites the missing chunk.
    const auto again = ingest(dir / "v1.bin");
    CHECK(again == ids1);
    CHECK(s.restoreToFile(ids1, (dir / "out4.bin").string()));
    CHECK(readRaw(dir / "out4.bin") == v1);

    // A new store object starts empty; scan() rebuilds the table and cleans interrupted writes.
    ChunkStore s2(root.string());
    CHECK(s2.chunkCount() == 0);
    const fs::path chunks = root / "chunks";
    writeRaw(chunks / (std::string(64, 'a') + ".chk.tmp"), makeBytes(100, 5));
    writeRaw(chunks / "notes.txt", makeBytes(5, 6));
    CHECK(s2.scan() == 11);
    CHECK(s2.chunkCount() == 11);
    CHECK(!anyTmpFiles(chunks));
    CHECK(fs::exists(chunks / "notes.txt")); // unrelated files are left alone
    CHECK(s2.has(ids2[3]));

    ChunkStore none((dir / "never_created").string());
    CHECK(none.scan() == 0);
}

static void testInterruptedChunkWrite(const fs::path& dir) {
    const fs::path root = dir / "store3";
    const Bytes c = makeBytes(4096, 77);
    const std::string cid = sha256Hex(c);

    ChunkStore s(root.string());
    // Simulate a crash in the middle of writing this chunk: only a partial temp file exists.
    writeRaw(s.chunkPath(cid) + ".tmp", Bytes(100, 0x11));

    ChunkStore after(root.string());
    after.scan(); // restart: temp file is removed
    CHECK(!after.has(cid));
    std::string id;
    bool isNew = false;
    Bytes back;
    CHECK(after.put(c, id, isNew)); // and the chunk can be stored normally afterwards
    CHECK(isNew && id == cid);
    CHECK(after.get(cid, back) && back == c);
    CHECK(!anyTmpFiles(root / "chunks"));
}

int main() {
    const fs::path dir = fs::temp_directory_path() / "recoverx_storage_test";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);

    testAtomicIo(dir / "atomic");
    testChunker(dir / "chunker");
    testStoreBasics(dir / "basics");
    testDedupRestoreScan(dir / "dedup");
    testInterruptedChunkWrite(dir / "crash");

    fs::remove_all(dir, ec);
    if (g_failures == 0) {
        std::cout << "storage tests passed\n";
        return 0;
    }
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
}
