#include "recoverx/verification.hpp"

#include "recoverx/chunkstore.hpp"
#include "recoverx/crc32.hpp"
#include "recoverx/sha256.hpp"

#include <chrono>
#include <limits>
#include <string>
#include <vector>

namespace recoverx {

bool verifyVersion(VersionInfo& version, const ChunkStore& store, VerifyReport& report) {
    const auto started = std::chrono::steady_clock::now();
    report = VerifyReport{};
    report.versionId = version.versionId;
    report.chunkCount = version.chunks.size();

    bool missing = false;
    bool corrupt = false;
    std::uint64_t describedSize = 0;

    for (std::size_t index = 0; index < version.chunks.size(); ++index) {
        const ChunkInfo& info = version.chunks[index];
        ChunkFailure failure;
        failure.index = index;

        if (!ChunkStore::isValidContentId(info.sha256)) {
            failure.invalidId = true;
            corrupt = true;
            report.failures.push_back(failure);
            continue;
        }
        if (info.size > std::numeric_limits<std::uint64_t>::max() - describedSize) {
            corrupt = true;
            report.totalSizeMismatch = true;
        } else {
            describedSize += info.size;
        }

        std::vector<std::uint8_t> bytes;
        if (!store.get(info.sha256, bytes)) {
            failure.missing = true;
            missing = true;
            report.failures.push_back(failure);
            continue;
        }
        failure.sizeMismatch = bytes.size() != info.size;
        failure.sha256Mismatch = sha256Hex(bytes) != info.sha256;
        failure.crc32Mismatch = crc32(bytes) != info.crc32;
        if (failure.sizeMismatch || failure.sha256Mismatch || failure.crc32Mismatch) {
            corrupt = true;
            report.failures.push_back(failure);
        }
    }

    if (describedSize != version.originalSize) {
        corrupt = true;
        report.totalSizeMismatch = true;
    }

    if (corrupt) {
        version.status = VersionStatus::Corrupt;
    } else if (missing) {
        version.status = VersionStatus::Missing;
    } else {
        version.status = VersionStatus::Valid;
    }
    report.status = version.status;
    report.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    return version.status == VersionStatus::Valid;
}

bool verifyVersion(VersionInfo& version, const ChunkStore& store) {
    VerifyReport ignored;
    return verifyVersion(version, store, ignored);
}

} // namespace recoverx
