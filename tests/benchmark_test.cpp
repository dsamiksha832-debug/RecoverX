#include "recoverx/benchmark.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;

int main() {
    const fs::path root = fs::temp_directory_path() / "recoverx_benchmark_test";
    std::error_code ec;
    fs::remove_all(root, ec);

    recoverx::BenchmarkReport report;
    std::string error;
    if (!recoverx::runBenchmarks((root / "reports").string(), 2, report, error)) {
        std::cerr << "benchmark failed: " << error << '\n';
        fs::remove_all(root, ec);
        return 1;
    }

    std::ifstream corruption(report.corruptionCsvPath, std::ios::binary);
    const std::string corruptionCsv(std::istreambuf_iterator<char>(corruption), {});
    std::size_t newlineCount = 0;
    for (const char c : corruptionCsv) if (c == '\n') ++newlineCount;
    if (newlineCount != 1 + 6 * 2 * 4 ||
        corruptionCsv.find("trial_id,method,corruption_mode,detected,data_size_bytes\n") != 0) {
        std::cerr << "corruption CSV has an unexpected header or number of rows\n";
        fs::remove_all(root, ec);
        return 1;
    }

    std::ifstream storage(report.storageCsvPath, std::ios::binary);
    const std::string storageCsv(std::istreambuf_iterator<char>(storage), {});
    if (storageCsv.find("file_id,version_count,original_bytes,full_copy_bytes,deduplicated_bytes,bytes_saved,storage_saved_percent\n") != 0 ||
        storageCsv.find("synthetic_file,4,") == std::string::npos) {
        std::cerr << "storage CSV has an unexpected header or result row\n";
        fs::remove_all(root, ec);
        return 1;
    }

    if (!fs::exists(report.reportPath) || !fs::exists(report.chartPath)) {
        std::cerr << "benchmark report or chart was not generated\n";
        fs::remove_all(root, ec);
        return 1;
    }
    std::ifstream summary(report.reportPath, std::ios::binary);
    const std::string markdown(std::istreambuf_iterator<char>(summary), {});
    if (markdown.find("## Corruption detection") == std::string::npos ||
        markdown.find("## Storage efficiency") == std::string::npos ||
        markdown.find("## Limitations") == std::string::npos) {
        std::cerr << "benchmark report is missing required sections\n";
        fs::remove_all(root, ec);
        return 1;
    }

    fs::remove_all(root, ec);
    std::cout << "benchmark tests passed\n";
    return 0;
}
