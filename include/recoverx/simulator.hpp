#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace recoverx {

// Apply the selected deterministic corruption mode to an in-memory test buffer.
bool corruptData(std::vector<std::uint8_t>& data, const std::string& mode);

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
