#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace recoverx {

// SHA-256 of `data` as exactly 64 lowercase hex characters (0-9, a-f).
// Used as the chunk identity (content id). Implemented by the Integrity member in src/sha256.cpp.
std::string sha256Hex(const std::vector<std::uint8_t>& data);

} // namespace recoverx
