#include "recoverx/chunker.hpp"

#include <stdexcept>

namespace recoverx {

ChunkReader::ChunkReader(const std::string& path, std::size_t chunkSize)
    : input_(path, std::ios::binary), chunkSize_(chunkSize) {
    if (chunkSize_ == 0) {
        throw std::invalid_argument("chunkSize must be greater than zero");
    }
    if (!input_) {
        throw std::runtime_error("Could not open input file: " + path);
    }
}

bool ChunkReader::next(std::vector<std::uint8_t>& chunk) {
    chunk.resize(chunkSize_);
    input_.read(reinterpret_cast<char*>(chunk.data()),
                static_cast<std::streamsize>(chunkSize_));
    const std::streamsize got = input_.gcount();

    if (input_.bad()) {
        throw std::runtime_error("Read error while chunking file");
    }
    if (got <= 0) {
        chunk.clear();
        return false;
    }

    chunk.resize(static_cast<std::size_t>(got)); // last chunk may be shorter
    bytesRead_ += static_cast<std::uint64_t>(got);
    return true;
}

std::vector<std::vector<std::uint8_t>> splitFile(
    const std::string& path,
    std::size_t chunkSize) {
    ChunkReader reader(path, chunkSize);
    std::vector<std::vector<std::uint8_t>> chunks;
    std::vector<std::uint8_t> chunk;
    while (reader.next(chunk)) {
        chunks.push_back(chunk);
    }
    return chunks;
}

} // namespace recoverx
