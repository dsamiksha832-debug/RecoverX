
#include "recoverx/crc32.hpp"
#include <array>

namespace recoverx {

std::uint32_t crc32(const std::vector<std::uint8_t>& data) {

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

    std::uint32_t crc = 0xFFFFFFFFu;

    for (const std::uint8_t byte : data) {
        const std::uint8_t index =
            static_cast<std::uint8_t>(crc ^ byte);

        crc = (crc >> 8) ^ table[index];
    }

    return crc ^ 0xFFFFFFFFu;
}

} // namespace recoverx
