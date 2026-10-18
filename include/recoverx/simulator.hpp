
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace recoverx {

// Corrupt test data in memory using the selected mode.
bool corruptData(std::vector<std::uint8_t>& data,
                 const std::string& mode);

// File-backed corruption will be connected to the storage
// implementation after the storage layout is agreed upon.
bool corruptStoredVersion(const std::string& fileName,
                          unsigned long long versionId,
                          const std::string& mode);

} // namespace recoverx
