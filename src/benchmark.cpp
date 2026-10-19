#include "recoverx/benchmark.hpp"

#include "recoverx/atomicio.hpp"
#include "recoverx/chunkstore.hpp"
#include "recoverx/checksum.hpp"
#include "recoverx/crc32.hpp"
#include "recoverx/registration.hpp"
#include "recoverx/sha256.hpp"
#include "recoverx/simulator.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

namespace recoverx {
namespace {

namespace fs = std::filesystem;
using Bytes = std::vector<std::uint8_t>;

class TempDirectory {
public:
    explicit TempDirectory(fs::path path) : path_(std::move(path)) {}
    ~TempDirectory() {
        std::error_code ignored;
        fs::remove_all(path_, ignored);
    }
    const fs::path& path() const { return path_; }

private:
    fs::path path_;
};

bool createUniqueTempDirectory(fs::path& result, std::string& error) {
    std::error_code ec;
    const fs::path base = fs::temp_directory_path(ec);
    if (ec) {
        error = "cannot locate a temporary directory";
        return false;
    }
    const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    for (unsigned attempt = 0; attempt < 100; ++attempt) {
        const fs::path candidate = base / ("recoverx_bench_" + std::to_string(stamp) +
                                           "_" + std::to_string(attempt));
        ec.clear();
        if (fs::create_directory(candidate, ec)) {
            result = candidate;
            return true;
        }
        if (ec && ec != std::errc::file_exists) {
            error = "cannot create temporary experiment data";
            return false;
        }
    }
    error = "cannot reserve a unique temporary experiment directory";
    return false;
}

std::uint64_t simpleSum(const Bytes& bytes) {
    std::uint64_t result = 0;
    for (const std::uint8_t byte : bytes) result += byte;
    return result;
}

std::string checksumValue(const std::string& method, const Bytes& bytes) {
    if (method == "simple_sum") return std::to_string(simpleSum(bytes));
    if (method == "internet_checksum") return std::to_string(internetChecksum(bytes));
    if (method == "crc32") return std::to_string(crc32(bytes));
    return sha256Hex(bytes);
}

bool writeCsv(const fs::path& path, const std::string& content) {
    const Bytes bytes(content.begin(), content.end());
    return atomicWriteFile(path.string(), bytes);
}

bool writeSyntheticFile(const fs::path& path, const Bytes& bytes) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    if (ec) return false;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    if (!bytes.empty()) {
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
    }
    out.flush();
    return static_cast<bool>(out);
}

bool runStorageExperiment(const fs::path& tempRoot, std::string& csv, std::string& error) {
    constexpr std::size_t chunkSize = 4096;
    constexpr std::size_t chunksPerVersion = 8;
    constexpr std::size_t versionCount = 4;
    const fs::path source = tempRoot / "storage" / "sample.bin";
    const fs::path storeRoot = tempRoot / "storage" / "store";

    Bytes data(chunkSize * chunksPerVersion);
    std::uint32_t state = 0xC001D00Du;
    for (std::size_t i = 0; i < data.size(); ++i) {
        state = state * 1664525u + 1013904223u;
        data[i] = static_cast<std::uint8_t>(state >> 24);
    }

    std::uint64_t fullCopyBytes = 0;
    for (std::size_t version = 0; version < versionCount; ++version) {
        if (version != 0) {
            const std::size_t chunk = version * 2;
            data[chunk * chunkSize + 17] ^= static_cast<std::uint8_t>(0x41u + version);
        }
        if (!writeSyntheticFile(source, data)) {
            error = "cannot create storage experiment input file";
            return false;
        }
        VersionInfo registered;
        if (!registerVersion(source.string(), storeRoot.string(), chunkSize, registered)) {
            error = "storage experiment could not register a generated version";
            return false;
        }
        if (data.size() > std::numeric_limits<std::uint64_t>::max() - fullCopyBytes) {
            error = "storage experiment size overflow";
            return false;
        }
        fullCopyBytes += static_cast<std::uint64_t>(data.size());
    }

    std::uint64_t deduplicatedBytes = 0;
    std::error_code ec;
    for (const char* subdir : {"chunks", "manifests"}) {
        const fs::path directory = fs::path(storeRoot) / subdir;
        fs::directory_iterator it(directory, ec);
        if (ec) {
            error = "cannot measure the generated deduplicated store";
            return false;
        }
        const fs::directory_iterator end;
        for (; it != end; it.increment(ec)) {
            if (ec) {
                error = "cannot measure the generated deduplicated store";
                return false;
            }
            std::error_code entryEc;
            if (!it->is_regular_file(entryEc) || entryEc) continue;
            if ((subdir == std::string("chunks") && it->path().extension() != ".chk") ||
                (subdir == std::string("manifests") && it->path().extension() != ".rxm")) continue;
            const std::uintmax_t fileBytes = it->file_size(entryEc);
            if (entryEc || fileBytes > std::numeric_limits<std::uint64_t>::max() - deduplicatedBytes) {
                error = "storage experiment size overflow";
                return false;
            }
            deduplicatedBytes += static_cast<std::uint64_t>(fileBytes);
        }
    }

    const std::uint64_t bytesSaved = fullCopyBytes > deduplicatedBytes
        ? fullCopyBytes - deduplicatedBytes : 0;
    csv = "file_id,version_count,original_bytes,full_copy_bytes,deduplicated_bytes,bytes_saved,storage_saved_percent\n";
    csv += "synthetic_file," + std::to_string(versionCount) + "," +
        std::to_string(data.size()) + "," + std::to_string(fullCopyBytes) + "," +
        std::to_string(deduplicatedBytes) + "," + std::to_string(bytesSaved) + ",";
    if (fullCopyBytes == 0) {
        csv += "NA\n";
    } else {
        const double percent = 100.0 * static_cast<double>(bytesSaved) /
                               static_cast<double>(fullCopyBytes);
        std::ostringstream value;
        value.precision(6);
        value << percent;
        csv += value.str() + "\n";
    }
    return true;
}

} // namespace

bool runBenchmarks(const std::string& outputDirectory,
                   std::size_t trialsPerMode,
                   BenchmarkReport& report,
                   std::string& error) {
    if (outputDirectory.empty() || trialsPerMode == 0) {
        error = "output directory and a positive trial count are required";
        return false;
    }

    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(outputDirectory, ec);
    if (ec || !fs::is_directory(outputDirectory, ec) || ec) {
        error = "cannot create benchmark output directory";
        return false;
    }

    fs::path temporaryPath;
    if (!createUniqueTempDirectory(temporaryPath, error)) return false;
    TempDirectory temporary(temporaryPath);

    constexpr std::size_t dataSize = 4096;
    const std::array<const char*, 4> methods{{"simple_sum", "internet_checksum", "crc32", "sha256"}};
    const std::array<const char*, 6> modes{{"bitflip", "burst", "swap", "truncate", "zero", "deletechunk"}};
    std::string corruptionCsv = "trial_id,method,corruption_mode,detected,data_size_bytes\n";
    std::uint64_t trialId = 0;
    for (const char* mode : modes) {
        for (std::size_t trial = 0; trial < trialsPerMode; ++trial) {
            Bytes original(dataSize);
            for (std::size_t i = 0; i < original.size(); ++i) {
                original[i] = static_cast<std::uint8_t>((i * 37u + trial * 13u + 19u) & 0xffu);
            }
            Bytes corrupted = original;
            if (!corruptData(corrupted, mode)) {
                error = std::string("failed to apply corruption mode: ") + mode;
                return false;
            }
            ++trialId;
            for (const char* method : methods) {
                const bool detected = checksumValue(method, original) != checksumValue(method, corrupted);
                corruptionCsv += std::to_string(trialId) + "," + method + "," + mode + "," +
                    (detected ? "true" : "false") + "," + std::to_string(dataSize) + "\n";
            }
        }
    }

    std::string storageCsv;
    if (!runStorageExperiment(temporary.path(), storageCsv, error)) return false;

    const fs::path corruptionPath = fs::path(outputDirectory) / "corruption_detection.csv";
    const fs::path storagePath = fs::path(outputDirectory) / "storage_efficiency.csv";
    if (!writeCsv(corruptionPath, corruptionCsv) || !writeCsv(storagePath, storageCsv)) {
        error = "cannot write benchmark CSV reports";
        return false;
    }

    BenchmarkReport result;
    result.corruptionCsvPath = corruptionPath.string();
    result.storageCsvPath = storagePath.string();
    result.trialsPerMode = trialsPerMode;
    report = std::move(result);
    return true;
}

} // namespace recoverx
