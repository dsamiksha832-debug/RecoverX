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

// Saves the current contents of sourcePath as an undo snapshot in a separate store at
// <storeRoot>/undo (its own chunks/ and manifests/), never listed as a backup version.
// Snapshot IDs increase within that store; the highest ID is the top of the undo stack.
bool snapshotForUndo(const std::string& sourcePath,
                     const std::string& storeRoot,
                     std::size_t chunkSize,
                     VersionInfo& snapshot);

} // namespace recoverx
