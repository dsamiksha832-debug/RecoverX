
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <cstddef>

namespace recoverx {

class SHA256 {
private:
    std::array<std::uint32_t, 8> state;
    std::array<std::uint8_t, 64> buffer;
    std::uint64_t totalBytes;
    std::size_t bufferSize;
    bool finalized;

public:
    SHA256();

    // Add another piece of data to the hash.
    void update(const std::uint8_t* data, std::size_t length);
    void update(const std::vector<std::uint8_t>& data);

    // Finish hashing and return 64 lowercase hexadecimal characters.
    std::string finalize();

};

// Existing interface: keep it working for the rest of the project.
std::string sha256Hex(const std::vector<std::uint8_t>& data);

} // namespace recoverx
