#include "recoverx/recovery.hpp"

#include "recoverx/chunkstore.hpp"
#include "recoverx/filehash.hpp"
#include "recoverx/manifest.hpp"
#include "recoverx/registration.hpp"
#include "recoverx/verification.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <system_error>
#include <utility>
#include <vector>

namespace recoverx {
namespace {

namespace fs = std::filesystem;

constexpr std::size_t kDefaultSnapshotChunk = 4096;

double secondsSince(std::chrono::steady_clock::time_point started) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
}

// "version_<digits>.rxm" -> id. Returns false for any other name.
bool parseManifestName(const std::string& name, std::uint64_t& id) {
    const std::string prefix = "version_";
    const std::string suffix = ".rxm";
    if (name.size() <= prefix.size() + suffix.size() ||
        name.compare(0, prefix.size(), prefix) != 0 ||
        name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0) {
        return false;
    }
    std::uint64_t value = 0;
    for (std::size_t i = prefix.size(); i < name.size() - suffix.size(); ++i) {
        const char c = name[i];
        if (c < '0' || c > '9' || value > 1000000000000000000ULL) return false;
        value = value * 10 + static_cast<unsigned>(c - '0');
    }
    id = value;
    return true;
}

// Reads every manifest in <storeRoot>/<folder>. Readable ones belonging to fileName go into
// `found` (newest first); unreadable ones are noted in `damaged`.
bool scanFolder(const std::string& fileName,
                const std::string& storeRoot,
                const char* folder,
                std::vector<VersionInfo>& found,
                std::vector<std::uint64_t>& damaged) {
    const fs::path directory = fs::path(storeRoot) / folder;
    std::error_code ec;
    if (!fs::is_directory(directory, ec) || ec) return false;

    fs::directory_iterator it(directory, ec);
    if (ec) return false;
    const fs::directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec) return false;
        std::error_code entryEc;
        if (!it->is_regular_file(entryEc)) continue;
        std::uint64_t id = 0;
        if (!parseManifestName(it->path().filename().string(), id)) continue;

        VersionInfo candidate;
        if (!readManifest(it->path().string(), candidate)) {
            damaged.push_back(id);
        } else if (sameFilePath(candidate.originalFileName, fileName)) {
            found.push_back(std::move(candidate));
        }
    }
    std::stable_sort(found.begin(), found.end(),
        [](const VersionInfo& a, const VersionInfo& b) { return a.versionId > b.versionId; });
    std::sort(damaged.begin(), damaged.end());
    return true;
}

std::string undoRoot(const std::string& storeRoot) {
    return (fs::path(storeRoot) / "undo").string();
}

fs::path undoManifestPath(const std::string& storeRoot, std::uint64_t id) {
    return fs::path(undoRoot(storeRoot)) / "manifests" / ("version_" + std::to_string(id) + ".rxm");
}

std::vector<std::string> contentIdsOf(const VersionInfo& version) {
    std::vector<std::string> ids;
    ids.reserve(version.chunks.size());
    for (const ChunkInfo& chunk : version.chunks) ids.push_back(chunk.sha256);
    return ids;
}

void fillRestoredInfo(const std::string& path, std::uint64_t& bytes, std::string& sha) {
    FileDigest digest;
    if (digestFile(path, digest)) {
        bytes = digest.size;
        sha = digest.sha256;
    }
}

// Pushes the current contents of outputPath (if the file exists) on the undo stack, then
// writes the verified version. If the write fails the undo entry is removed again.
bool restoreVerified(const VersionInfo& version,
                     const std::string& storeRoot,
                     const std::string& outputPath,
                     const ChunkStore& store,
                     RecoveryReport& report) {
    std::error_code ec;
    VersionInfo snapshot;
    bool saved = false;
    if (fs::is_regular_file(outputPath, ec)) {
        if (!snapshotForUndo(outputPath, storeRoot, kDefaultSnapshotChunk, snapshot)) {
            report.error = "could not save the current file on the undo stack; nothing was changed";
            return false;
        }
        saved = true;
    }

    if (!store.restoreToFile(contentIdsOf(version), outputPath)) {
        if (saved) {
            fs::remove(undoManifestPath(storeRoot, snapshot.versionId), ec);
        }
        report.error = "writing the restored file failed; the original file was not changed";
        return false;
    }

    report.success = true;
    report.restoredVersionId = version.versionId;
    report.undoSaved = saved;
    report.undoId = saved ? snapshot.versionId : 0;
    fillRestoredInfo(outputPath, report.restoredBytes, report.restoredSha256);
    return true;
}

} // namespace

bool listVersions(const std::string& fileName,
                  const std::string& storeRoot,
                  std::vector<VersionInfo>& versions,
                  std::vector<std::uint64_t>& damagedManifests) {
    versions.clear();
    damagedManifests.clear();
    if (fileName.empty() || storeRoot.empty()) return false;
    return scanFolder(fileName, storeRoot, "manifests", versions, damagedManifests);
}

bool recoverFile(const std::string& fileName,
                 const std::string& storeRoot,
                 RecoveryReport& report,
                 std::uint64_t targetVersion) {
    const auto started = std::chrono::steady_clock::now();
    report = RecoveryReport{};
    if (fileName.empty() || storeRoot.empty()) {
        report.error = "file name and store are required";
        return false;
    }

    std::vector<VersionInfo> candidates;
    if (!listVersions(fileName, storeRoot, candidates, report.damagedManifests)) {
        report.error = "no backup store found";
        return false;
    }
    if (targetVersion != 0) {
        candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
            [targetVersion](const VersionInfo& v) { return v.versionId != targetVersion; }),
            candidates.end());
    }
    if (candidates.empty()) {
        report.error = targetVersion != 0
            ? "that version does not exist for this file, or its manifest is damaged"
            : "no backup versions were found for this file";
        report.seconds = secondsSince(started);
        return false;
    }

    ChunkStore store(storeRoot);
    store.scan();
    for (VersionInfo& candidate : candidates) {
        VerifyReport verify;
        const bool valid = verifyVersion(candidate, store, verify);
        report.attempts.push_back(verify);
        if (!valid) continue;
        restoreVerified(candidate, storeRoot, fileName, store, report);
        report.seconds = secondsSince(started);
        return report.success;
    }

    report.error = targetVersion != 0
        ? "the requested version failed verification"
        : "no valid version exists; every stored version failed verification";
    report.seconds = secondsSince(started);
    return false;
}

bool restoreVersion(std::uint64_t versionId,
                    const std::string& storeRoot,
                    const std::string& outputPath,
                    RecoveryReport& report) {
    const auto started = std::chrono::steady_clock::now();
    report = RecoveryReport{};
    if (versionId == 0 || storeRoot.empty() || outputPath.empty()) {
        report.error = "version id, store and output path are required";
        return false;
    }

    const fs::path manifestPath = fs::path(storeRoot) / "manifests" /
        ("version_" + std::to_string(versionId) + ".rxm");
    VersionInfo version;
    if (!readManifest(manifestPath.string(), version) || version.versionId != versionId) {
        report.error = "the manifest is missing or damaged (its own CRC-32 failed)";
        report.seconds = secondsSince(started);
        return false;
    }

    ChunkStore store(storeRoot);
    store.scan();
    VerifyReport verify;
    const bool valid = verifyVersion(version, store, verify);
    report.attempts.push_back(verify);
    if (!valid) {
        report.error = "the version failed verification and was not restored";
        report.seconds = secondsSince(started);
        return false;
    }
    restoreVerified(version, storeRoot, outputPath, store, report);
    report.seconds = secondsSince(started);
    return report.success;
}

bool undoRestore(const std::string& fileName,
                 const std::string& storeRoot,
                 UndoReport& report) {
    const auto started = std::chrono::steady_clock::now();
    report = UndoReport{};
    if (fileName.empty() || storeRoot.empty()) {
        report.error = "file name and store are required";
        return false;
    }

    std::vector<VersionInfo> stack; // newest (top) first
    std::vector<std::uint64_t> damaged;
    if (!scanFolder(fileName, undoRoot(storeRoot), "manifests", stack, damaged) || stack.empty()) {
        report.error = "nothing to undo for this file";
        return false;
    }

    VersionInfo& top = stack.front();
    ChunkStore store(undoRoot(storeRoot));
    store.scan();
    if (!verifyVersion(top, store)) {
        report.error = "the saved previous state failed verification and cannot be restored";
        report.snapshotId = top.versionId;
        return false;
    }
    if (!store.restoreToFile(contentIdsOf(top), fileName)) {
        report.error = "writing the file failed; the current file was not changed";
        return false;
    }

    std::error_code ec;
    fs::remove(undoManifestPath(storeRoot, top.versionId), ec);

    report.success = true;
    report.snapshotId = top.versionId;
    report.remaining = stack.size() - 1;
    fillRestoredInfo(fileName, report.restoredBytes, report.restoredSha256);
    report.seconds = secondsSince(started);
    return true;
}

bool recoverLatestValidVersion(const std::string& /*fileName*/) {
    // No default store location is part of the public project contract.
    return false;
}

bool recoverLatestValidVersion(const std::string& fileName,
                               const std::string& storeRoot) {
    RecoveryReport report;
    return recoverFile(fileName, storeRoot, report, 0);
}

bool restoreVersion(std::uint64_t versionId,
                    const std::string& storeRoot,
                    const std::string& outputPath) {
    RecoveryReport report;
    return restoreVersion(versionId, storeRoot, outputPath, report);
}

bool undoLastChange(const std::string& fileName,
                    const std::string& storeRoot) {
    UndoReport report;
    return undoRestore(fileName, storeRoot, report);
}

} // namespace recoverx
