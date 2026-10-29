#pragma once
#include "recoverx/common.hpp"
#include "recoverx/verification.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace recoverx {

// Everything a recover/restore did. The library never prints; the CLI formats this.
struct RecoveryReport {
    bool success = false;
    std::string error;                           // set when success is false
    std::uint64_t restoredVersionId = 0;
    std::vector<VerifyReport> attempts;          // versions checked, newest first
    std::vector<std::uint64_t> damagedManifests; // manifests skipped because their CRC-32 failed
    bool undoSaved = false;                      // the previous file state was pushed on the undo stack
    std::uint64_t undoId = 0;
    std::uint64_t restoredBytes = 0;
    std::string restoredSha256;                  // SHA-256 of the file that was written
    double seconds = 0.0;
};

struct UndoReport {
    bool success = false;
    std::string error;
    std::uint64_t snapshotId = 0;
    std::uint64_t restoredBytes = 0;
    std::string restoredSha256;
    std::size_t remaining = 0; // snapshots still on the undo stack for this file
    double seconds = 0.0;
};

// All backup versions of fileName, newest first. Manifests whose own CRC-32 fails cannot be
// trusted: they are not returned, and their version ids are listed in `damagedManifests`.
// Returns false if the store has no manifest folder.
bool listVersions(const std::string& fileName,
                  const std::string& storeRoot,
                  std::vector<VersionInfo>& versions,
                  std::vector<std::uint64_t>& damagedManifests);

// Walks versions newest to oldest, verifying each, and restores the first valid one to
// fileName. If targetVersion is not 0, only that version is tried (no fallback).
// If fileName already exists, its current contents are pushed on the undo stack first.
bool recoverFile(const std::string& fileName,
                 const std::string& storeRoot,
                 RecoveryReport& report,
                 std::uint64_t targetVersion = 0);

// Verifies one stored version and restores it to outputPath. If outputPath already exists,
// its current contents are pushed on the undo stack first.
bool restoreVersion(std::uint64_t versionId,
                    const std::string& storeRoot,
                    const std::string& outputPath,
                    RecoveryReport& report);

// Pops the top undo snapshot saved for fileName and restores it. Undo does not push, so
// repeated undo walks back through earlier states.
bool undoRestore(const std::string& fileName,
                 const std::string& storeRoot,
                 UndoReport& report);

// Simple forms without a report.
bool recoverLatestValidVersion(const std::string& fileName);
bool recoverLatestValidVersion(const std::string& fileName,
                               const std::string& storeRoot);
bool restoreVersion(std::uint64_t versionId,
                    const std::string& storeRoot,
                    const std::string& outputPath);
bool undoLastChange(const std::string& fileName,
                    const std::string& storeRoot);

} // namespace recoverx
