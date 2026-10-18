#pragma once

#include "recoverx/common.hpp"

#include <cstddef>
#include <string>

namespace recoverx {

// Streams sourcePath into the content-addressed store and persists its metadata.
// Manifest IDs increase globally within storeRoot. On failure, registeredVersion
// is unchanged and no manifest is published.
bool registerVersion(const std::string& sourcePath,
                     const std::string& storeRoot,
                     std::size_t chunkSize,
                     VersionInfo& registeredVersion);

} // namespace recoverx
