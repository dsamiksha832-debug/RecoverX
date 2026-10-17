#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace recoverx {

// Reads a file in binary mode and returns fixed-size chunks.
// Starter implementation is intentionally simple and loads the file into memory;
// replace with streaming for the final large-file requirement.
std::vector<std::vector<std::uint8_t>> splitFile(
    const std::string& path,
    std::size_t chunkSize);

} // namespace recoverx
