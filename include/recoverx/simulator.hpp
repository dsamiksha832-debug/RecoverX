#pragma once
#include <string>

namespace recoverx {

// Placeholder: later support bitflip, burst, swap, truncate, zero, deletechunk.
bool corruptStoredVersion(const std::string& fileName,
                          unsigned long long versionId,
                          const std::string& mode);

} // namespace recoverx
