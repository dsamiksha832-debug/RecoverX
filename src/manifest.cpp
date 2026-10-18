#include "recoverx/manifest.hpp"

#include "recoverx/atomicio.hpp"
#include "recoverx/crc32.hpp"

#include <array>
#include <filesystem>
#include <limits>
#include <system_error>
#include <vector>

namespace recoverx {
namespace {

constexpr std::array<std::uint8_t, 4> kMagic{{'R', 'X', 'M', 'F'}};
constexpr std::uint32_t kFormatVersion = 1;
constexpr std::size_t kMaxManifestBytes = 64u * 1024u * 1024u;
constexpr std::uint32_t kMaxFileNameBytes = 1024u * 1024u;
constexpr std::uint64_t kMaxChunks = 1u << 20;

void appendU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void appendU64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (unsigned shift = 0; shift < 64; shift += 8) {
        out.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

bool takeU32(const std::vector<std::uint8_t>& bytes, std::size_t& pos,
             std::uint32_t& value) {
    if (bytes.size() - pos < 4) return false;
    value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= std::uint32_t(bytes[pos++]) << (8 * i);
    return true;
}

bool takeU64(const std::vector<std::uint8_t>& bytes, std::size_t& pos,
             std::uint64_t& value) {
    if (bytes.size() - pos < 8) return false;
    value = 0;
    for (unsigned i = 0; i < 8; ++i) value |= std::uint64_t(bytes[pos++]) << (8 * i);
    return true;
}

} // namespace

bool writeManifest(const std::string& path, const VersionInfo& version) {
    if (version.originalFileName.size() > kMaxFileNameBytes ||
        version.chunks.size() > kMaxChunks ||
        version.originalFileName.size() > std::numeric_limits<std::uint32_t>::max() ||
        static_cast<int>(version.status) < static_cast<int>(VersionStatus::Unverified) ||
        static_cast<int>(version.status) > static_cast<int>(VersionStatus::Missing)) {
        return false;
    }

    std::vector<std::uint8_t> bytes(kMagic.begin(), kMagic.end());
    appendU32(bytes, kFormatVersion);
    appendU64(bytes, version.versionId);
    appendU64(bytes, version.originalSize);
    bytes.push_back(static_cast<std::uint8_t>(version.status));
    appendU32(bytes, static_cast<std::uint32_t>(version.originalFileName.size()));
    appendU64(bytes, static_cast<std::uint64_t>(version.chunks.size()));
    bytes.insert(bytes.end(), version.originalFileName.begin(), version.originalFileName.end());

    for (const ChunkInfo& chunk : version.chunks) {
        if (chunk.sha256.size() != 64) return false;
        for (const char c : chunk.sha256) {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
        }
        bytes.insert(bytes.end(), chunk.sha256.begin(), chunk.sha256.end());
        appendU32(bytes, chunk.crc32);
        appendU64(bytes, chunk.size);
        if (bytes.size() > kMaxManifestBytes - 4) return false;
    }

    appendU32(bytes, crc32(bytes));
    return atomicWriteFile(path, bytes);
}

bool readManifest(const std::string& path, VersionInfo& version) {
    std::error_code ec;
    const std::uintmax_t fileSize = std::filesystem::file_size(path, ec);
    if (ec || fileSize > kMaxManifestBytes) return false;

    std::vector<std::uint8_t> bytes;
    if (!readFileBytes(path, bytes) || bytes.size() < 4 + 4 + 8 + 8 + 1 + 4 + 8 + 4 ||
        bytes.size() > kMaxManifestBytes) {
        return false;
    }

    const std::size_t payloadSize = bytes.size() - 4;
    std::size_t crcPos = payloadSize;
    std::uint32_t storedCrc = 0;
    if (!takeU32(bytes, crcPos, storedCrc) ||
        crc32(std::vector<std::uint8_t>(bytes.begin(), bytes.begin() + payloadSize)) != storedCrc) {
        return false;
    }

    for (std::size_t i = 0; i < kMagic.size(); ++i) {
        if (bytes[i] != kMagic[i]) return false;
    }
    std::size_t pos = kMagic.size();
    std::uint32_t format = 0, nameSize = 0;
    std::uint64_t versionId = 0, originalSize = 0, chunkCount = 0;
    if (!takeU32(bytes, pos, format) || format != kFormatVersion ||
        !takeU64(bytes, pos, versionId) || !takeU64(bytes, pos, originalSize) ||
        pos >= payloadSize) return false;
    const std::uint8_t rawStatus = bytes[pos++];
    if (rawStatus > static_cast<std::uint8_t>(VersionStatus::Missing) ||
        !takeU32(bytes, pos, nameSize) || nameSize > kMaxFileNameBytes ||
        !takeU64(bytes, pos, chunkCount) || chunkCount > kMaxChunks ||
        nameSize > payloadSize - pos) return false;

    VersionInfo parsed;
    parsed.versionId = versionId;
    parsed.originalSize = originalSize;
    parsed.status = static_cast<VersionStatus>(rawStatus);
    parsed.originalFileName.assign(bytes.begin() + pos, bytes.begin() + pos + nameSize);
    pos += nameSize;
    // Each chunk record has a fixed 76-byte encoding; validate the full size before allocation.
    if (chunkCount > (payloadSize - pos) / 76 || pos + chunkCount * 76 != payloadSize) return false;
    parsed.chunks.reserve(static_cast<std::size_t>(chunkCount));
    for (std::uint64_t i = 0; i < chunkCount; ++i) {
        ChunkInfo chunk;
        chunk.sha256.assign(bytes.begin() + pos, bytes.begin() + pos + 64);
        pos += 64;
        for (const char c : chunk.sha256) {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
        }
        if (!takeU32(bytes, pos, chunk.crc32) || !takeU64(bytes, pos, chunk.size)) return false;
        parsed.chunks.push_back(std::move(chunk));
    }
    version = std::move(parsed);
    return true;
}

} // namespace recoverx
