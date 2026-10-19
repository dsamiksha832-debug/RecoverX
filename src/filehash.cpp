#include "recoverx/filehash.hpp"

#include "recoverx/checksum.hpp"
#include "recoverx/crc32.hpp"
#include "recoverx/sha256.hpp"

#include <fstream>
#include <vector>

namespace recoverx {

bool digestFile(const std::string& path, FileDigest& digest) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    // The block size must be even: the Internet checksum adds 16-bit words, so only the
    // final block may have an odd length. For an even-length block, ~internetChecksum(block)
    // is that block's one's-complement sum, and one's-complement sums can be added with
    // end-around carry.
    constexpr std::size_t kBlock = 64 * 1024;
    std::vector<std::uint8_t> block(kBlock);
    SHA256 sha;
    FileDigest result;
    std::uint32_t sum = 0;

    while (in) {
        in.read(reinterpret_cast<char*>(block.data()), static_cast<std::streamsize>(kBlock));
        const std::streamsize got = in.gcount();
        if (got <= 0) break;
        block.resize(static_cast<std::size_t>(got));

        result.size += static_cast<std::uint64_t>(got);
        result.crc32 = crc32Continue(result.crc32, block.data(), block.size());
        sha.update(block);

        sum += static_cast<std::uint16_t>(~internetChecksum(block));
        sum = (sum & 0xFFFFu) + (sum >> 16);

        block.resize(kBlock);
    }
    if (in.bad()) return false;

    while (sum >> 16) sum = (sum & 0xFFFFu) + (sum >> 16);
    result.internetChecksum = static_cast<std::uint16_t>(~sum & 0xFFFFu);
    result.sha256 = sha.finalize();
    digest = result;
    return true;
}

} // namespace recoverx
