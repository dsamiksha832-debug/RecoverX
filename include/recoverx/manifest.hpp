#pragma once
#include "recoverx/common.hpp"
#include <string>

namespace recoverx {

// Versioned binary manifest. Writes use a temporary file and atomic replacement.
bool writeManifest(const std::string& path, const VersionInfo& version);
bool readManifest(const std::string& path, VersionInfo& version);

} // namespace recoverx
