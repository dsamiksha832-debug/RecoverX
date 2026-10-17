#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace recoverx {

// Educational Internet checksum over bytes, returned as a 16-bit value.
// This is error detection, not cryptographic security.
std::uint16_t internetChecksum(const std::vector<std::uint8_t>& data);

} // namespace recoverx
