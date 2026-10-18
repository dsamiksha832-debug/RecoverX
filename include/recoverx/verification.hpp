#pragma once

#include "recoverx/common.hpp"

namespace recoverx {

class ChunkStore;

// Verifies manifest metadata and each stored chunk's size, SHA-256, and CRC-32.
// Sets status to Valid only if every check succeeds; returns true only then.
// Missing chunks produce Missing. Malformed metadata or mismatched contents
// produce Corrupt. If both occur, Corrupt takes precedence.
bool verifyVersion(VersionInfo& version, const ChunkStore& store);

} // namespace recoverx
