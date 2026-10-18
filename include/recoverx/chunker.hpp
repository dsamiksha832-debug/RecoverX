#pragma once
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace recoverx {

// Streaming reader: returns a binary file one fixed-size chunk at a time.
// Memory use is one chunk, not the whole file (large-file requirement).
class ChunkReader {
public:
    // Throws std::invalid_argument if chunkSize == 0 and std::runtime_error if the file
    // cannot be opened.
    ChunkReader(const std::string& path, std::size_t chunkSize);

    // Fills `chunk` with the next chunk (at most chunkSize bytes; the last one may be shorter).
    // Returns false at end of file (`chunk` is then empty). Throws std::runtime_error on a
    // read error.
    bool next(std::vector<std::uint8_t>& chunk);

    std::uint64_t bytesRead() const { return bytesRead_; }

private:
    std::ifstream input_;
    std::size_t chunkSize_;
    std::uint64_t bytesRead_ = 0;
};

// Convenience: reads the whole file as chunks held in memory. Fine for tests and small files;
// use ChunkReader for large files. An empty file gives an empty list.
std::vector<std::vector<std::uint8_t>> splitFile(
    const std::string& path,
    std::size_t chunkSize);

} // namespace recoverx
