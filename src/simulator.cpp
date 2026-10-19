#include "recoverx/simulator.hpp"

#include "recoverx/atomicio.hpp"
#include "recoverx/chunkstore.hpp"
#include "recoverx/manifest.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>
#include <unordered_set>
#include <vector>

namespace recoverx {
namespace {

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool isSupportedMode(const std::string& mode) {
    return mode == "bitflip" || mode == "burst" || mode == "swap" ||
           mode == "truncate" || mode == "zero" || mode == "deletechunk";
}

} // namespace

bool corruptData(std::vector<std::uint8_t>& data, const std::string& mode) {
    if (data.empty()) return false;
    if (mode == "bitflip") {
        data[0] ^= 0x01u;
        return true;
    }
    if (mode == "burst") {
        data[0] ^= 0x07u;
        return true;
    }
    if (mode == "swap") {
        if (data.size() < 2) return false;
        std::swap(data.front(), data.back());
        return true;
    }
    if (mode == "truncate") {
        data.pop_back();
        return true;
    }
    if (mode == "zero") {
        std::fill(data.begin(), data.end(), 0);
        return true;
    }
    if (mode == "deletechunk") {
        data.clear();
        return true;
    }
    return false;
}

bool corruptStoredVersion(const std::string& /*fileName*/,
                          unsigned long long /*versionId*/,
                          const std::string& /*mode*/) {
    // The compatibility signature has no store root, so it cannot locate safe target data.
    return false;
}

bool corruptStoredVersion(const std::string& fileName,
                          const std::string& storeRoot,
                          unsigned long long versionId,
                          const std::string& mode) {
    if (fileName.empty() || storeRoot.empty() || versionId == 0) return false;
    const std::string normalizedMode = lowercase(mode);
    if (!isSupportedMode(normalizedMode)) return false;

    namespace fs = std::filesystem;
    const fs::path manifestDirectory = fs::path(storeRoot) / "manifests";
    const fs::path targetManifest = manifestDirectory /
        ("version_" + std::to_string(versionId) + ".rxm");
    VersionInfo target;
    if (!readManifest(targetManifest.string(), target) ||
        target.versionId != versionId || target.originalFileName != fileName ||
        target.chunks.empty()) {
        return false;
    }

    // Content-addressed chunks may be shared by different versions. Do not mutate
    // a chunk if another valid manifest refers to it.
    std::unordered_set<std::string> referencedByOtherVersions;
    std::error_code ec;
    fs::directory_iterator it(manifestDirectory, ec);
    if (ec) return false;
    const fs::directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec) return false;
        std::error_code entryEc;
        if (!it->is_regular_file(entryEc) || it->path() == targetManifest ||
            it->path().extension() != ".rxm") continue;
        VersionInfo other;
        if (!readManifest(it->path().string(), other)) continue;
        for (const ChunkInfo& chunk : other.chunks) {
            referencedByOtherVersions.insert(chunk.sha256);
        }
    }

    ChunkStore store(storeRoot);
    store.scan();
    std::string selectedId;
    std::vector<std::uint8_t> bytes;
    bool found = false;
    for (const ChunkInfo& chunk : target.chunks) {
        if (referencedByOtherVersions.count(chunk.sha256) != 0 ||
            !ChunkStore::isValidContentId(chunk.sha256) ||
            !store.get(chunk.sha256, bytes)) continue;
        selectedId = chunk.sha256;
        found = true;
        break;
    }
    if (!found) return false;

    if (normalizedMode == "deletechunk") {
        return fs::remove(store.chunkPath(selectedId), ec) && !ec;
    }
    if (bytes.empty()) return false;

    switch (normalizedMode.front()) {
        case 'b':
            if (normalizedMode == "bitflip") {
                bytes[0] ^= 0x01u;
            } else {
                bytes[0] ^= 0x07u;
            }
            break;
        case 's':
            if (bytes.size() < 2 || bytes.front() == bytes.back()) return false;
            std::swap(bytes.front(), bytes.back());
            break;
        case 't':
            bytes.pop_back();
            break;
        case 'z': {
            const bool alreadyZero = std::all_of(bytes.begin(), bytes.end(),
                [](std::uint8_t byte) { return byte == 0; });
            if (alreadyZero) return false;
            std::fill(bytes.begin(), bytes.end(), 0);
            break;
        }
        default:
            return false;
    }

    return atomicWriteFile(store.chunkPath(selectedId), bytes);
}

} // namespace recoverx
