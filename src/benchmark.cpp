#include "recoverx/benchmark.hpp"

#include "recoverx/atomicio.hpp"
#include "recoverx/chunkstore.hpp"
#include "recoverx/checksum.hpp"
#include "recoverx/crc32.hpp"
#include "recoverx/registration.hpp"
#include "recoverx/sha256.hpp"
#include "recoverx/simulator.hpp"

#include <algorithm>
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

struct StorageMetrics {
    std::uint64_t originalBytes = 0;
    std::uint64_t fullCopyBytes = 0;
    std::uint64_t deduplicatedBytes = 0;
    std::uint64_t bytesSaved = 0;
    double savedPercent = 0;
};

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

bool runStorageExperiment(const fs::path& tempRoot, std::string& csv,
                          StorageMetrics& metrics, std::string& error) {
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
    metrics.originalBytes = static_cast<std::uint64_t>(data.size());
    metrics.fullCopyBytes = fullCopyBytes;
    metrics.deduplicatedBytes = deduplicatedBytes;
    metrics.bytesSaved = bytesSaved;
    metrics.savedPercent = fullCopyBytes == 0 ? 0.0 :
        100.0 * static_cast<double>(bytesSaved) / static_cast<double>(fullCopyBytes);
    csv = "file_id,version_count,original_bytes,full_copy_bytes,deduplicated_bytes,bytes_saved,storage_saved_percent\n";
    csv += "synthetic_file," + std::to_string(versionCount) + "," +
        std::to_string(data.size()) + "," + std::to_string(fullCopyBytes) + "," +
        std::to_string(deduplicatedBytes) + "," + std::to_string(bytesSaved) + ",";
    if (fullCopyBytes == 0) {
        csv += "NA\n";
    } else {
        std::ostringstream value;
        value.precision(6);
        value << metrics.savedPercent;
        csv += value.str() + "\n";
    }
    return true;
}

std::string makeDetectionChart(
    const std::array<const char*, 4>& methods,
    const std::array<const char*, 6>& modes,
    const std::array<std::array<std::size_t, 4>, 6>& detected,
    std::size_t trialsPerMode,
    double storageSavedPercent) {
    const std::array<const char*, 4> colors{{"#2563eb", "#16a34a", "#f59e0b", "#9333ea"}};
    std::ostringstream svg;
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"900\" height=\"560\" viewBox=\"0 0 900 560\">\n"
        << "<rect width=\"900\" height=\"560\" fill=\"white\"/>\n"
        << "<text x=\"450\" y=\"28\" text-anchor=\"middle\" font-family=\"sans-serif\" font-size=\"20\">RecoverX benchmark results</text>\n"
        << "<text x=\"28\" y=\"205\" transform=\"rotate(-90 28 205)\" text-anchor=\"middle\" font-family=\"sans-serif\" font-size=\"13\">Detection rate (%)</text>\n";
    constexpr int chartTop = 62;
    constexpr int chartHeight = 250;
    constexpr int chartBottom = chartTop + chartHeight;
    for (int tick = 0; tick <= 100; tick += 20) {
        const int y = chartBottom - chartHeight * tick / 100;
        svg << "<line x1=\"74\" y1=\"" << y << "\" x2=\"850\" y2=\"" << y
            << "\" stroke=\"#d1d5db\"/><text x=\"64\" y=\"" << y + 4
            << "\" text-anchor=\"end\" font-family=\"sans-serif\" font-size=\"11\">"
            << tick << "</text>\n";
    }
    for (std::size_t mode = 0; mode < modes.size(); ++mode) {
        const int groupX = 112 + static_cast<int>(mode) * 122;
        for (std::size_t method = 0; method < methods.size(); ++method) {
            const double rate = trialsPerMode == 0 ? 0.0 :
                100.0 * static_cast<double>(detected[mode][method]) /
                static_cast<double>(trialsPerMode);
            const int barHeight = static_cast<int>(chartHeight * rate / 100.0);
            const int x = groupX + static_cast<int>(method) * 18;
            svg << "<rect x=\"" << x << "\" y=\"" << chartBottom - barHeight
                << "\" width=\"13\" height=\"" << barHeight << "\" fill=\""
                << colors[method] << "\"><title>" << modes[mode] << " / " << methods[method]
                << ": " << rate << "%</title></rect>\n";
        }
        svg << "<text x=\"" << groupX + 28 << "\" y=\"" << chartBottom + 18
            << "\" text-anchor=\"middle\" font-family=\"sans-serif\" font-size=\"10\">"
            << modes[mode] << "</text>\n";
    }
    for (std::size_t method = 0; method < methods.size(); ++method) {
        const int x = 130 + static_cast<int>(method) * 175;
        svg << "<rect x=\"" << x << "\" y=\"354\" width=\"12\" height=\"12\" fill=\""
            << colors[method] << "\"/><text x=\"" << x + 18
            << "\" y=\"365\" font-family=\"sans-serif\" font-size=\"12\">"
            << methods[method] << "</text>\n";
    }
    const double boundedSaved = std::max(0.0, std::min(100.0, storageSavedPercent));
    svg << "<text x=\"75\" y=\"425\" font-family=\"sans-serif\" font-size=\"14\">Storage saved with chunk deduplication: "
        << storageSavedPercent << "%</text>\n"
        << "<rect x=\"75\" y=\"442\" width=\"775\" height=\"24\" rx=\"4\" fill=\"#e5e7eb\"/>\n"
        << "<rect x=\"75\" y=\"442\" width=\"" << 775.0 * boundedSaved / 100.0
        << "\" height=\"24\" rx=\"4\" fill=\"#0f766e\"/>\n"
        << "<text x=\"75\" y=\"495\" font-family=\"sans-serif\" font-size=\"12\">Synthetic workload; "
        << trialsPerMode << " trials per corruption mode. See the CSV files and Markdown report for details.</text>\n"
        << "</svg>\n";
    return svg.str();
}

std::string makeReport(const std::array<const char*, 4>& methods,
                       const std::array<const char*, 6>& modes,
                       const std::array<std::array<std::size_t, 4>, 6>& detected,
                       std::size_t trialsPerMode,
                       const StorageMetrics& storage) {
    std::ostringstream report;
    report << "# RecoverX benchmark report\n\n"
           << "Generated by `recoverx bench`. The corruption experiment uses deterministic synthetic data of 4,096 bytes and compares the simple sum, Internet checksum, CRC-32, and SHA-256 before and after each corruption. It runs "
           << trialsPerMode << " trials per corruption mode. The storage experiment registers four 32,768-byte versions with 4,096-byte chunks; one new chunk changes in each later version. Deduplicated size includes chunk and manifest files.\n\n"
           << "## Corruption detection\n\n"
           << "| Mode | Simple sum | Internet checksum | CRC-32 | SHA-256 |\n|---|---:|---:|---:|---:|\n";
    for (std::size_t mode = 0; mode < modes.size(); ++mode) {
        report << "| " << modes[mode];
        for (std::size_t method = 0; method < methods.size(); ++method) {
            const double rate = trialsPerMode == 0 ? 0.0 :
                100.0 * static_cast<double>(detected[mode][method]) /
                static_cast<double>(trialsPerMode);
            report << " | " << detected[mode][method] << "/" << trialsPerMode
                   << " (" << rate << "%)";
        }
        report << " |\n";
    }
    report << "\n## Storage efficiency\n\n"
           << "- Versions: " << 4 << "\n"
           << "- Original file size: " << storage.originalBytes << " bytes\n"
           << "- Full-copy total: " << storage.fullCopyBytes << " bytes\n"
           << "- RecoverX chunks and manifests: " << storage.deduplicatedBytes << " bytes\n"
           << "- Bytes saved: " << storage.bytesSaved << " (" << storage.savedPercent << "%)\n\n"
           << "## Limitations\n\n"
           << "These results describe the generated workload only. Detection rates and storage savings vary with real file contents and change patterns. The command makes no prediction about untested data. The CSV files contain the row-level measurements; `benchmark_summary.svg` visualizes the measured detection rates and storage savings.\n";
    return report.str();
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
    std::array<std::array<std::size_t, 4>, 6> detectedCounts{};
    std::uint64_t trialId = 0;
    for (std::size_t modeIndex = 0; modeIndex < modes.size(); ++modeIndex) {
        const char* mode = modes[modeIndex];
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
            for (std::size_t methodIndex = 0; methodIndex < methods.size(); ++methodIndex) {
                const char* method = methods[methodIndex];
                const bool detected = checksumValue(method, original) != checksumValue(method, corrupted);
                if (detected) ++detectedCounts[modeIndex][methodIndex];
                corruptionCsv += std::to_string(trialId) + "," + method + "," + mode + "," +
                    (detected ? "true" : "false") + "," + std::to_string(dataSize) + "\n";
            }
        }
    }

    std::string storageCsv;
    StorageMetrics storage;
    if (!runStorageExperiment(temporary.path(), storageCsv, storage, error)) return false;

    const fs::path corruptionPath = fs::path(outputDirectory) / "corruption_detection.csv";
    const fs::path storagePath = fs::path(outputDirectory) / "storage_efficiency.csv";
    const fs::path reportPath = fs::path(outputDirectory) / "benchmark_report.md";
    const fs::path chartPath = fs::path(outputDirectory) / "benchmark_summary.svg";
    if (!writeCsv(corruptionPath, corruptionCsv) || !writeCsv(storagePath, storageCsv) ||
        !writeCsv(reportPath, makeReport(methods, modes, detectedCounts, trialsPerMode, storage)) ||
        !writeCsv(chartPath, makeDetectionChart(methods, modes, detectedCounts,
                                                trialsPerMode, storage.savedPercent))) {
        error = "cannot write benchmark CSV reports";
        return false;
    }

    BenchmarkReport result;
    result.corruptionCsvPath = corruptionPath.string();
    result.storageCsvPath = storagePath.string();
    result.reportPath = reportPath.string();
    result.chartPath = chartPath.string();
    result.trialsPerMode = trialsPerMode;
    report = std::move(result);
    return true;
}

} // namespace recoverx
