#include "recoverx/chunkstore.hpp"
#include "recoverx/registration.hpp"
#include "recoverx/simulator.hpp"
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
    const fs::path root = fs::temp_directory_path() / "recoverx_simulator_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);

    const std::string modes[] = {"bitflip", "burst", "swap", "truncate", "zero", "deletechunk"};
    Bytes original(256);
    for (std::size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<std::uint8_t>((i * 19 + 11) & 0xff);
    }

    for (const std::string& mode : modes) {
        const fs::path scenario = root / mode;
        const fs::path source = scenario / "source.bin";
        const fs::path storeRoot = scenario / "store";
        writeBytes(source, original);
        VersionInfo version;
        CHECK(registerVersion(source.string(), storeRoot.string(), 256, version));
        CHECK(corruptStoredVersion(source.string(), storeRoot.string(),
                                   version.versionId, mode));
        CHECK(readBytes(source) == original); // simulator must never change the source

        ChunkStore store(storeRoot.string());
        store.scan();
        CHECK(!verifyVersion(version, store));
        CHECK(version.status == (mode == "deletechunk" ? VersionStatus::Missing
                                                        : VersionStatus::Corrupt));
    }

    const fs::path sharedSource = root / "shared-source.bin";
    const fs::path sharedStore = root / "shared-store";
    writeBytes(sharedSource, original);
    VersionInfo sharedOne;
    VersionInfo sharedTwo;
    CHECK(registerVersion(sharedSource.string(), sharedStore.string(), 256, sharedOne));
    CHECK(registerVersion(sharedSource.string(), sharedStore.string(), 256, sharedTwo));
    CHECK(!corruptStoredVersion(sharedSource.string(), sharedStore.string(),
                                sharedTwo.versionId, "bitflip"));
    CHECK(!corruptStoredVersion(sharedSource.string(), sharedStore.string(),
                                sharedTwo.versionId, "unknown"));
    ChunkStore sharedChunks(sharedStore.string());
    sharedChunks.scan();
    CHECK(verifyVersion(sharedOne, sharedChunks));
    CHECK(readBytes(sharedSource) == original);

    fs::remove_all(root, ec);
    if (failures == 0) {
        std::cout << "simulator tests passed\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
