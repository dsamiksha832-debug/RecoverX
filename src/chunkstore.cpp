#include "recoverx/chunkstore.hpp"

namespace recoverx {

ChunkStore::ChunkStore(std::string rootDirectory)
    : rootDirectory_(std::move(rootDirectory)) {}

bool ChunkStore::put(const std::vector<std::uint8_t>& /*bytes*/,
                     std::string& /*contentId*/) {
    // TODO: hash content with SHA-256, check whether it exists, then safely write.
    return false;
}

bool ChunkStore::get(const std::string& /*contentId*/,
                     std::vector<std::uint8_t>& /*bytes*/) const {
    // TODO: validate contentId, read bytes, and let caller verify integrity.
    return false;
}

} // namespace recoverx
