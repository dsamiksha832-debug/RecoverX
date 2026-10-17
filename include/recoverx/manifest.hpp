#pragma once
#include "recoverx/common.hpp"
#include <string>

namespace recoverx {

// Placeholder API contract. Manifest writes must later use temp-file + rename.
bool writeManifest(const std::string& path, const VersionInfo& version);
bool readManifest(const std::string& path, VersionInfo& version);

} // namespace recoverx
