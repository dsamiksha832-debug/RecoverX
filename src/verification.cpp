#include "recoverx/verification.hpp"

#include "recoverx/chunkstore.hpp"
#include "recoverx/crc32.hpp"
#include "recoverx/sha256.hpp"

#include <limits>
#include <string>
#include <vector>

namespace recoverx {

bool verifyVersion(VersionInfo& version, const ChunkStore& store) {
    bool missing = false;
    bool corrupt = false;
    std::uint64_t describedSize = 0;

    for (const ChunkInfo& info : version.chunks) {
        if (!ChunkStore::isValidContentId(info.sha256)) {
            corrupt = true;
            continue;
        }
        if (info.size > std::numeric_limits<std::uint64_t>::max() - describedSize) {
            corrupt = true;
        } else {
            describedSize += info.size;
        }

        std::vector<std::uint8_t> bytes;
        if (!store.get(info.sha256, bytes)) {
            missing = true;
            continue;
        }
        if (bytes.size() != info.size || sha256Hex(bytes) != info.sha256 ||
            crc32(bytes) != info.crc32) {
            corrupt = true;
        }
    }

    if (describedSize != version.originalSize) corrupt = true;

    if (corrupt) {
        version.status = VersionStatus::Corrupt;
        return false;
    }
    if (missing) {
        version.status = VersionStatus::Missing;
        return false;
    }
    version.status = VersionStatus::Valid;
    return true;
}

} // namespace recoverx
