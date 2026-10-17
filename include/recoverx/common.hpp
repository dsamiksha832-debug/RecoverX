#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace recoverx {

enum class VersionStatus {
    Unverified,
    Valid,
    Corrupt,
    Missing
};

inline const char* toString(VersionStatus status) {
    switch (status) {
        case VersionStatus::Unverified: return "UNVERIFIED";
        case VersionStatus::Valid: return "VALID";
        case VersionStatus::Corrupt: return "CORRUPT";
        case VersionStatus::Missing: return "MISSING";
    }
    return "UNKNOWN";
}

struct ChunkInfo {
    std::string sha256;
    std::uint32_t crc32 = 0;
    std::uint64_t size = 0;
};

struct VersionInfo {
    std::uint64_t versionId = 0;
    std::string originalFileName;
    std::uint64_t originalSize = 0;
    std::vector<ChunkInfo> chunks;
    VersionStatus status = VersionStatus::Unverified;
};

} // namespace recoverx
