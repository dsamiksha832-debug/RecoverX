#pragma once
#include <cstdint>
#include <string>

namespace recoverx {

// Compatibility entry point. A store root is required for recovery; this overload
// returns false because no default store location is defined by the project.
bool recoverLatestValidVersion(const std::string& fileName);

// Finds manifests for the exact fileName, checks versions newest first, and
// atomically restores the first valid version to fileName.
bool recoverLatestValidVersion(const std::string& fileName,
                               const std::string& storeRoot);

// Verifies and restores one stored version to outputPath atomically.
bool restoreVersion(std::uint64_t versionId,
                    const std::string& storeRoot,
                    const std::string& outputPath);

// Skips the newest snapshot for fileName and restores the newest valid earlier one.
bool undoLastChange(const std::string& fileName,
                    const std::string& storeRoot);

} // namespace recoverx
