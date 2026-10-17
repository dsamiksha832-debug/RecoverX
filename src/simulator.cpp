
#include "recoverx/simulator.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace recoverx {

// Apply corruption to test data only.
// This function does not open or modify any files.
bool corruptData(std::vector<std::uint8_t>& data,
                 const std::string& mode) {
    if (data.empty()) {
        return false;
    }

    if (mode == "bitflip") {
        // Change one bit in the first byte.
        data[0] = data[0] ^ 1;
        return true;
    }

    if (mode == "burst") {
        // Flip several nearby bits in the first byte.
        data[0] = data[0] ^ 0x07;
        return true;
    }

    if (mode == "swap") {
        // Swap the first and last bytes.
        if (data.size() < 2) {
            return false;
        }

        std::uint8_t temp = data[0];
        data[0] = data[data.size() - 1];
        data[data.size() - 1] = temp;
        return true;
    }

    if (mode == "truncate") {
        // Remove the last byte.
        data.pop_back();
        return true;
    }

    if (mode == "zero") {
        // Replace all bytes with zero.
        for (std::size_t i = 0; i < data.size(); i++) {
            data[i] = 0;
        }
        return true;
    }

    if (mode == "deletechunk") {
        // Simulate deletion by clearing the test chunk.
        data.clear();
        return true;
    }

    // Unknown corruption mode.
    return false;
}

bool corruptStoredVersion(const std::string& /*fileName*/,
                          unsigned long long /*versionId*/,
                          const std::string& /*mode*/) {
    // File-backed corruption is not connected yet.
    // Do not guess the storage path or modify original files.
    return false;
}

} // namespace recoverx
