#include "recoverx/crc32.hpp"
#include "recoverx/checksum.hpp"
#include "recoverx/versionchain.hpp"
#include "recoverx/sha256.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

int main() {
    const std::string text = "123456789";
    const std::vector<std::uint8_t> bytes(text.begin(), text.end());

    // Standard CRC-32/ISO-HDLC known-answer test.
    assert(recoverx::crc32(bytes) == 0xCBF43926u);

    // Empty history is safe and returns an empty snapshot.
    recoverx::VersionChain chain;
    assert(chain.size() == 0);
    assert(chain.newestFirst().empty());

    recoverx::VersionInfo v1;
    v1.versionId = 1;
    v1.originalFileName = "sample.txt";
    chain.append(v1);
    assert(chain.size() == 1);
    const auto oneVersion = chain.newestFirst();
    assert(oneVersion.size() == 1);
    assert(oneVersion[0].versionId == 1);

    recoverx::VersionInfo v2;
    v2.versionId = 2;
    v2.originalFileName = "sample.txt";
    chain.append(v2);

    assert(chain.size() == 2);
    const auto newest = chain.newestFirst();
    assert(newest.size() == 2);
    assert(newest[0].versionId == 2);
    assert(newest[1].versionId == 1);

    // Appending after traversing preserves tail linkage and newest-first order.
    recoverx::VersionInfo v3;
    v3.versionId = 3;
    v3.originalFileName = "sample.txt";
    chain.append(v3);
    assert(chain.size() == 3);
    const auto threeNewest = chain.newestFirst();
    assert(threeNewest.size() == 3);
    assert(threeNewest[0].versionId == 3);
    assert(threeNewest[1].versionId == 2);
    assert(threeNewest[2].versionId == 1);
    // Returned vectors are snapshots; later appends do not mutate prior results.
    assert(newest.size() == 2);
    assert(newest[0].versionId == 2);

    // Make sure checksum API is linked and deterministic.
    const auto c1 = recoverx::internetChecksum(bytes);
    const auto c2 = recoverx::internetChecksum(bytes);
    assert(c1 == c2);
    assert(c1 == 0xF62Au);

    const std::vector<std::uint8_t> empty;
    assert(recoverx::internetChecksum(empty) == 0xFFFFu);
    assert(recoverx::crc32(empty) == 0u);
    assert(recoverx::sha256Hex(empty) ==
           "e3b0c44298fc1c149afbf4c8996fb924"
           "27ae41e4649b934ca495991b7852b855");

    const std::string abc = "abc";
    const std::vector<std::uint8_t> abcBytes(abc.begin(), abc.end());
    assert(recoverx::sha256Hex(abcBytes) ==
           "ba7816bf8f01cfea414140de5dae2223"
           "b00361a396177a9cb410ff61f20015ad");

    recoverx::SHA256 incrementalHash;
    incrementalHash.update(bytes.data(), 4);
    incrementalHash.update(bytes.data() + 4, bytes.size() - 4);
    const std::string incrementalResult = incrementalHash.finalize();
    assert(incrementalResult == recoverx::sha256Hex(bytes));
    assert(incrementalResult ==
           "15e2b0d3c33891ebb0f1ef609ec41942"
           "0c20e320ce94c65fbc8c3312448eb225");

    const std::size_t lengths[] = {55, 56, 63, 64, 65, 127, 128, 129};
    const std::string expectedHashes[] = {
        "8aa994584139d128848eeebc4e815639ba5ab6e6e39574195a63ac4f14f7c43b",
        "ad574708f75c044c9b85de64cb568ee7711ff4f36448c6242f053ba8f6cc2b63",
        "280ed3e8ff1df845b2e7dfe6ac6cee817bef20e783cc65abc41b818b4d2fe076",
        "c6ab9724ade5b6a7a1edfffb12f3aa9181351355af8fd08c919952ad211339dd",
        "788367c73c7ddf4c53f65e68cc0d943e6227ab55b0e78ba63ace822b1c6301c0",
        "192409cd280e14b743642ad1343fbd3e82d9305de72c078117745a679210cc3d",
        "cc548ca2dec1f6fe4f58b2e27aa9c7521607df1130d140b55a4dad0665302356",
        "81e89a7b2911aaa7795f9e3d4910cb47d6cd2b00d83b8399481527261a1a7519"
    };
    for (std::size_t test = 0; test < 8; ++test) {
        std::vector<std::uint8_t> data(lengths[test]);
        for (std::size_t i = 0; i < data.size(); ++i) {
            data[i] = static_cast<std::uint8_t>(i * 31u + 7u);
        }
        assert(recoverx::sha256Hex(data) == expectedHashes[test]);

        recoverx::SHA256 hash;
        for (std::size_t offset = 0; offset < data.size();) {
            const std::size_t count = std::min<std::size_t>(7, data.size() - offset);
            hash.update(data.data() + offset, count);
            offset += count;
        }
        assert(hash.finalize() == expectedHashes[test]);
    }

    const std::vector<std::uint8_t> oneByte{0x01};
    const std::vector<std::uint8_t> threeBytes{0x01, 0x02, 0x03};
    assert(recoverx::internetChecksum(oneByte) == 0xFEFFu);
    assert(recoverx::internetChecksum(threeBytes) == 0xFBFDu);

    recoverx::SHA256 finalizedHash;
    finalizedHash.update(abcBytes);
    finalizedHash.finalize();
    bool updateRejected = false;
    try {
        finalizedHash.update(abcBytes);
    } catch (const std::logic_error&) {
        updateRejected = true;
    }
    assert(updateRejected);
    bool finalizeRejected = false;
    try {
        finalizedHash.finalize();
    } catch (const std::logic_error&) {
        finalizeRejected = true;
    }
    assert(finalizeRejected);

    return 0;
}
