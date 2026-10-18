#include "recoverx/crc32.hpp"
#include "recoverx/checksum.hpp"
#include "recoverx/versionchain.hpp"

#include <cassert>
#include <cstdint>
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

    return 0;
}
