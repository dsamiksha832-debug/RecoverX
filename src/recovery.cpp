#include "recoverx/recovery.hpp"

#include "recoverx/chunkstore.hpp"
#include "recoverx/manifest.hpp"
#include "recoverx/verification.hpp"

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <utility>
#include <vector>

namespace recoverx {
namespace {

bool loadVersionsForFile(const std::string& fileName,
                         const std::string& storeRoot,
                         std::vector<VersionInfo>& candidates) {
    namespace fs = std::filesystem;
    const fs::path manifestDirectory = fs::path(storeRoot) / "manifests";
    std::error_code ec;
    if (!fs::is_directory(manifestDirectory, ec) || ec) return false;

    fs::directory_iterator it(manifestDirectory, ec);
    if (ec) return false;
    const fs::directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec) return false;
        std::error_code entryEc;
        if (!it->is_regular_file(entryEc) || it->path().extension() != ".rxm") continue;

        VersionInfo candidate;
        if (readManifest(it->path().string(), candidate) &&
            candidate.originalFileName == fileName) {
            candidates.push_back(std::move(candidate));
        }
    }
    std::stable_sort(candidates.begin(), candidates.end(),
        [](const VersionInfo& left, const VersionInfo& right) {
            return left.versionId > right.versionId;
        });
    return true;
}

bool restoreFirstValid(std::vector<VersionInfo>& candidates,
                       std::size_t begin,
                       const std::string& storeRoot,
                       const std::string& outputPath) {
    ChunkStore store(storeRoot);
    store.scan();
    for (std::size_t i = begin; i < candidates.size(); ++i) {
        VersionInfo& candidate = candidates[i];
        if (!verifyVersion(candidate, store)) continue;

        std::vector<std::string> contentIds;
        contentIds.reserve(candidate.chunks.size());
        for (const ChunkInfo& chunk : candidate.chunks) contentIds.push_back(chunk.sha256);
        if (store.restoreToFile(contentIds, outputPath)) return true;
    }
    return false;
}

} // namespace

bool recoverLatestValidVersion(const std::string& /*fileName*/) {
    // No default store location is part of the public project contract.
    return false;
}

bool recoverLatestValidVersion(const std::string& fileName,
                               const std::string& storeRoot) {
    if (fileName.empty() || storeRoot.empty()) return false;

    std::vector<VersionInfo> candidates;
    if (!loadVersionsForFile(fileName, storeRoot, candidates)) return false;
    return restoreFirstValid(candidates, 0, storeRoot, fileName);
}

bool restoreVersion(std::uint64_t versionId,
                    const std::string& storeRoot,
                    const std::string& outputPath) {
    if (versionId == 0 || storeRoot.empty() || outputPath.empty()) return false;

    namespace fs = std::filesystem;
    const fs::path manifestPath = fs::path(storeRoot) / "manifests" /
        ("version_" + std::to_string(versionId) + ".rxm");
    VersionInfo version;
    if (!readManifest(manifestPath.string(), version) || version.versionId != versionId) {
        return false;
    }

    ChunkStore store(storeRoot);
    store.scan();
    if (!verifyVersion(version, store)) return false;

    std::vector<std::string> contentIds;
    contentIds.reserve(version.chunks.size());
    for (const ChunkInfo& chunk : version.chunks) contentIds.push_back(chunk.sha256);
    return store.restoreToFile(contentIds, outputPath);
}

bool undoLastChange(const std::string& fileName,
                    const std::string& storeRoot) {
    if (fileName.empty() || storeRoot.empty()) return false;

    std::vector<VersionInfo> candidates;
    if (!loadVersionsForFile(fileName, storeRoot, candidates) || candidates.size() < 2) {
        return false;
    }
    return restoreFirstValid(candidates, 1, storeRoot, fileName);
}

} // namespace recoverx
