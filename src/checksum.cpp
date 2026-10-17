#include "recoverx/checksum.hpp"

namespace recoverx {

std::uint16_t internetChecksum(const std::vector<std::uint8_t>& data) {
    std::uint32_t sum = 0;
    std::size_t i = 0;

    while (i + 1 < data.size()) {
        const std::uint16_t word =
            static_cast<std::uint16_t>((data[i] << 8) | data[i + 1]);
        sum += word;
        sum = (sum & 0xFFFFu) + (sum >> 16);
        i += 2;
    }

    if (i < data.size()) {
        sum += static_cast<std::uint16_t>(data[i] << 8);
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }

    return static_cast<std::uint16_t>(~sum & 0xFFFFu);
}

} // namespace recoverx
