#pragma once
#include <cstdint>
#include <string>

namespace recoverx {

struct FileDigest {
    std::uint64_t size = 0;
    std::uint32_t crc32 = 0;
    std::uint16_t internetChecksum = 0;
    std::string sha256; // 64 lowercase hex characters
};

// Streams a file once (64 KB at a time, so memory use does not depend on file size) and
// computes its CRC-32, Internet checksum and SHA-256. Returns false if the file cannot be read.
bool digestFile(const std::string& path, FileDigest& digest);

} // namespace recoverx
