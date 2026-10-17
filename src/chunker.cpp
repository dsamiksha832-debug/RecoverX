#include "recoverx/chunker.hpp"

#include <fstream>
#include <iterator>
#include <stdexcept>

namespace recoverx {

std::vector<std::vector<std::uint8_t>> splitFile(
    const std::string& path,
    std::size_t chunkSize) {
    if (chunkSize == 0) {
        throw std::invalid_argument("chunkSize must be greater than zero");
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not open input file: " + path);
    }

    std::vector<std::uint8_t> bytes(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());

    std::vector<std::vector<std::uint8_t>> chunks;
    for (std::size_t offset = 0; offset < bytes.size(); offset += chunkSize) {
        const std::size_t end = (offset + chunkSize < bytes.size())
            ? offset + chunkSize : bytes.size();
        chunks.emplace_back(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                            bytes.begin() + static_cast<std::ptrdiff_t>(end));
    }
    return chunks;
}

} // namespace recoverx
