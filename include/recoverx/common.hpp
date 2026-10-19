#pragma once

#include <cstdint>
#include <filesystem>
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

// True if two path strings name the same file as typed by the user ("a.txt" and "./a.txt").
// Purely textual: it does not touch the disk.
inline bool sameFilePath(const std::string& left, const std::string& right) {
    return std::filesystem::path(left).lexically_normal() ==
           std::filesystem::path(right).lexically_normal();
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
