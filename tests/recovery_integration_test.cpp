#include "recoverx/chunkstore.hpp"
#include "recoverx/manifest.hpp"
#include "recoverx/recovery.hpp"
#include "recoverx/registration.hpp"

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
    const fs::path root = fs::temp_directory_path() / "recoverx_recovery_integration_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);

    const fs::path storeRoot = root / "store";
    const fs::path source = root / "document.bin";
    const fs::path exportPath = root / "exported.bin";
    Bytes original(9000);
    for (std::size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<std::uint8_t>((i * 31 + 7) & 0xff);
    }
    Bytes edited = original;
    edited[17] ^= 0x7f;
    edited[6000] ^= 0xa5;

    // A failed source read must not publish a manifest.
    VersionInfo failed;
    CHECK(!registerVersion((root / "absent.bin").string(), storeRoot.string(), 2048, failed));
    CHECK(!fs::exists(storeRoot / "manifests" / "version_1.rxm"));

    writeBytes(source, original);
    VersionInfo v1;
    CHECK(registerVersion(source.string(), storeRoot.string(), 2048, v1));
    writeBytes(source, edited);
    VersionInfo v2;
    CHECK(registerVersion(source.string(), storeRoot.string(), 2048, v2));
    CHECK(v2.versionId > v1.versionId);

    // Explicit restore works for a selected version and preserves output on validation failure.
    CHECK(restoreVersion(v1.versionId, storeRoot.string(), exportPath.string()));
    CHECK(readBytes(exportPath) == original);
    const Bytes sentinel{'s', 'a', 'f', 'e'};
    writeBytes(exportPath, sentinel);

    ChunkStore store(storeRoot.string());
    store.scan();
    Bytes damaged = readBytes(store.chunkPath(v2.chunks[0].sha256));
    damaged[0] ^= 0x01;
    writeBytes(store.chunkPath(v2.chunks[0].sha256), damaged);
    CHECK(!restoreVersion(v2.versionId, storeRoot.string(), exportPath.string()));
    CHECK(readBytes(exportPath) == sentinel);

    // Recovery chooses v1 after rejecting damaged v2 and safely replaces the source.
    CHECK(recoverLatestValidVersion(source.string(), storeRoot.string()));
    CHECK(readBytes(source) == original);

    // Recovery pushed the file's previous contents (the edited text) on the undo stack,
    // so undo brings exactly that state back.
    CHECK(undoLastChange(source.string(), storeRoot.string()));
    CHECK(readBytes(source) == edited);
    // The stack is now empty.
    CHECK(!undoLastChange(source.string(), storeRoot.string()));

    // A malformed unrelated manifest is ignored during history discovery.
    writeBytes(storeRoot / "manifests" / "version_999.rxm", Bytes{'b', 'a', 'd'});
    CHECK(recoverLatestValidVersion(source.string(), storeRoot.string()));
    CHECK(readBytes(source) == original);

    fs::remove_all(root, ec);
    if (failures == 0) {
        std::cout << "recovery integration tests passed\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
