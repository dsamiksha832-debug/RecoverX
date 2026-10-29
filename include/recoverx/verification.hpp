#pragma once

#include "recoverx/common.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace recoverx {

class ChunkStore;

// One chunk that failed verification. `index` is 0-based. Several flags can be set at once.
struct ChunkFailure {
    std::size_t index = 0;
    bool invalidId = false;      // the manifest holds a malformed SHA-256 id
    bool missing = false;        // the chunk file is not in the store
    bool sizeMismatch = false;   // stored size differs from the manifest
    bool sha256Mismatch = false; // SHA-256 of the stored bytes differs from the manifest
    bool crc32Mismatch = false;  // CRC-32 of the stored bytes differs from the manifest
};

// What verification found. The library never prints; the CLI turns this into a report.
struct VerifyReport {
    std::uint64_t versionId = 0;
    VersionStatus status = VersionStatus::Unverified;
    std::size_t chunkCount = 0;
    bool totalSizeMismatch = false; // chunk sizes do not add up to the recorded file size
    std::vector<ChunkFailure> failures;
    double seconds = 0.0;
};

// Verifies manifest metadata and each stored chunk's size, SHA-256, and CRC-32.
// Sets status to Valid only if every check succeeds; returns true only then.
// Missing chunks produce Missing. Malformed metadata or mismatched contents
// produce Corrupt. If both occur, Corrupt takes precedence.
bool verifyVersion(VersionInfo& version, const ChunkStore& store);

// Same check, and fills `report` with which chunks failed and which checks caught them.
bool verifyVersion(VersionInfo& version, const ChunkStore& store, VerifyReport& report);

} // namespace recoverx
