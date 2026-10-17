#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace recoverx {

class ChunkStore {
public:
    explicit ChunkStore(std::string rootDirectory);

    // Placeholder API contract; implementation will store chunks by SHA-256.
    bool put(const std::vector<std::uint8_t>& bytes, std::string& contentId);
    bool get(const std::string& contentId, std::vector<std::uint8_t>& bytes) const;

private:
    std::string rootDirectory_;
};

} // namespace recoverx
