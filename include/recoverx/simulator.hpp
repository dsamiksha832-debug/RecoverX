#pragma once
#include <string>

namespace recoverx {

// Compatibility entry point. A store root is required to locate stored chunks.
bool corruptStoredVersion(const std::string& fileName,
                          unsigned long long versionId,
                          const std::string& mode);

// Applies a controlled corruption to a chunk referenced only by this version.
// Supported modes: bitflip, burst, swap, truncate, zero, deletechunk.
// The original source file is never modified.
bool corruptStoredVersion(const std::string& fileName,
                          const std::string& storeRoot,
                          unsigned long long versionId,
                          const std::string& mode);

} // namespace recoverx
