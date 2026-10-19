#include "recoverx/registration.hpp"

#include "recoverx/chunker.hpp"
#include "recoverx/chunkstore.hpp"
#include "recoverx/crc32.hpp"
#include "recoverx/manifest.hpp"

#include <filesystem>
#include <limits>
#include <system_error>

namespace recoverx {
namespace {

bool parseManifestId(const std::string& name, std::uint64_t& id) {
    constexpr char prefix[] = "version_";
    constexpr char suffix[] = ".rxm";
    if (name.size() <= sizeof(prefix) - 1 + sizeof(suffix) - 1 ||
        name.compare(0, sizeof(prefix) - 1, prefix) != 0 ||
        name.compare(name.size() - (sizeof(suffix) - 1), sizeof(suffix) - 1, suffix) != 0) {
        return false;
    }
    const std::string digits = name.substr(sizeof(prefix) - 1,
        name.size() - (sizeof(prefix) - 1) - (sizeof(suffix) - 1));
    std::uint64_t value = 0;
    for (const char c : digits) {
        if (c < '0' || c > '9' || value > (std::numeric_limits<std::uint64_t>::max() - 9) / 10) return false;
        value = value * 10 + static_cast<unsigned>(c - '0');
    }
    id = value;
    return true;
}

} // namespace

namespace {

// Streams sourcePath into the store at storeRoot and publishes its manifest.
// Backups use <store>; undo snapshots use <store>/undo, a separate store, so a corrupted
// backup chunk can never make an undo snapshot unrestorable (and vice versa).
bool registerInto(const std::string& sourcePath,
                  const std::string& storeRoot,
                  std::size_t chunkSize,
                  VersionInfo& registeredVersion) {
    if (chunkSize == 0 || sourcePath.empty() || storeRoot.empty()) return false;

    namespace fs = std::filesystem;
    const fs::path manifests = fs::path(storeRoot) / "manifests";
    std::error_code ec;
    fs::create_directories(manifests, ec);
    if (ec) return false;

    std::uint64_t maxId = 0;
    bool foundId = false;
    fs::directory_iterator it(manifests, ec);
    if (ec) return false;
    const fs::directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec) return false;
        std::error_code entryEc;
        if (!it->is_regular_file(entryEc)) continue;
        std::uint64_t id = 0;
        if (parseManifestId(it->path().filename().string(), id)) {
            foundId = true;
            if (id > maxId) maxId = id;
        }
    }
    if (foundId && maxId == std::numeric_limits<std::uint64_t>::max()) return false;

    VersionInfo candidate;
    candidate.versionId = foundId ? maxId + 1 : 1;
    candidate.originalFileName = sourcePath;
    candidate.status = VersionStatus::Unverified;

    try {
        ChunkReader reader(sourcePath, chunkSize);
        ChunkStore store(storeRoot);
        store.scan();
        std::vector<std::uint8_t> bytes;
        while (reader.next(bytes)) {
            std::string id;
            if (!store.put(bytes, id)) return false;
            if (bytes.size() > std::numeric_limits<std::uint64_t>::max() - candidate.originalSize) return false;
            candidate.originalSize += static_cast<std::uint64_t>(bytes.size());
            candidate.chunks.push_back(ChunkInfo{id, crc32(bytes), static_cast<std::uint64_t>(bytes.size())});
        }
    } catch (...) {
        return false;
    }

    const fs::path manifestPath = manifests / ("version_" + std::to_string(candidate.versionId) + ".rxm");
    if (!writeManifest(manifestPath.string(), candidate)) return false;
    registeredVersion = std::move(candidate);
    return true;
}

} // namespace

bool registerVersion(const std::string& sourcePath,
                     const std::string& storeRoot,
                     std::size_t chunkSize,
                     VersionInfo& registeredVersion) {
    return registerInto(sourcePath, storeRoot, chunkSize, registeredVersion);
}

bool snapshotForUndo(const std::string& sourcePath,
                     const std::string& storeRoot,
                     std::size_t chunkSize,
                     VersionInfo& snapshot) {
    namespace fs = std::filesystem;
    return registerInto(sourcePath, (fs::path(storeRoot) / "undo").string(), chunkSize, snapshot);
}

} // namespace recoverx
