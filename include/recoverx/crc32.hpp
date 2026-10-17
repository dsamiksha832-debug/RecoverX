#pragma once
#include <cstdint>
#include <vector>

namespace recoverx {

// CRC-32/ISO-HDLC: reflected polynomial 0xEDB88320.
std::uint32_t crc32(const std::vector<std::uint8_t>& data);

} // namespace recoverx
