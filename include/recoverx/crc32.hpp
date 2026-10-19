#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace recoverx {

// CRC-32/ISO-HDLC: reflected polynomial 0xEDB88320.
std::uint32_t crc32(const std::vector<std::uint8_t>& data);

// Incremental form for streaming. Start with previousCrc = 0 and feed the data in order;
// the returned value is the CRC-32 of everything fed so far.
std::uint32_t crc32Continue(std::uint32_t previousCrc, const std::uint8_t* data, std::size_t length);

} // namespace recoverx
