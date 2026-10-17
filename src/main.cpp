#include "recoverx/crc32.hpp"
#include "recoverx/checksum.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    const std::vector<std::uint8_t> sample{'R', 'e', 'c', 'o', 'v', 'e', 'r', 'X'};

    std::cout << "RecoverX - C++17 starter\n";
    std::cout << "CRC-32: 0x" << std::hex << recoverx::crc32(sample) << '\n';
    std::cout << "Internet checksum: 0x" << recoverx::internetChecksum(sample) << '\n';
    std::cout << std::dec << "\nBuild and smoke test passed. Backup workflow is not implemented yet.\n";
    return 0;
}
