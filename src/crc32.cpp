
#include "recoverx/crc32.hpp"
#include <array>

namespace recoverx {

std::uint32_t crc32Continue(std::uint32_t previousCrc, const std::uint8_t* data, std::size_t length) {

    static const std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> t{};

        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;

            for (int bit = 0; bit < 8; ++bit) {
                if (c & 1u) {
                    c = (c >> 1) ^ 0xEDB88320u;
                } else {
                    c >>= 1;
                }
            }

            t[i] = c;
        }

        return t;
    }();

    std::uint32_t crc = previousCrc ^ 0xFFFFFFFFu;

    for (std::size_t i = 0; i < length; ++i) {
        const std::uint8_t index =
            static_cast<std::uint8_t>(crc ^ data[i]);

        crc = (crc >> 8) ^ table[index];
    }

    return crc ^ 0xFFFFFFFFu;
}

std::uint32_t crc32(const std::vector<std::uint8_t>& data) {
    return crc32Continue(0u, data.data(), data.size());
}

} // namespace recoverx
