#pragma once
#include <string>

namespace recoverx {

// Returns true only if a valid version was restored.
// Full recovery workflow will be implemented after storage/manifest modules.
bool recoverLatestValidVersion(const std::string& fileName);

} // namespace recoverx
