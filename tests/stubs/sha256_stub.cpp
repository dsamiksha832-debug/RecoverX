// TEST ONLY - THIS IS NOT SHA-256 AND MUST NEVER BE USED IN THE REAL PROGRAM.
// Placeholder so the storage tests can run before the Integrity member merges src/sha256.cpp.
// It returns 64 lowercase hex characters, like the real function, so storage code behaves the same.
// CMake stops using this file automatically once src/sha256.cpp exists.
#include "recoverx/sha256.hpp"

#include <cstdio>

namespace recoverx {

std::string sha256Hex(const std::vector<std::uint8_t>& data) {
    std::uint64_t h[4] = {0xcbf29ce484222325ULL, 0x9e3779b97f4a7c15ULL,
                          0xc2b2ae3d27d4eb4fULL, 0x165667b19e3779f9ULL};
    const std::uint64_t prime = 0x100000001b3ULL;

    for (const std::uint8_t b : data) {
        for (int k = 0; k < 4; ++k) {
            h[k] = (h[k] ^ static_cast<std::uint64_t>(b + 1 + 37 * k)) * prime;
            h[k] ^= h[k] >> 29;
        }
    }

    std::string out;
    char buf[17];
    for (int k = 0; k < 4; ++k) {
        h[k] ^= static_cast<std::uint64_t>(data.size()) * 0x9e3779b97f4a7c15ULL;
        std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(h[k]));
        out += buf;
    }
    return out;
}

} // namespace recoverx
