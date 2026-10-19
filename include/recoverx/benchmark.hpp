#pragma once

#include <cstddef>
#include <string>

namespace recoverx {

struct BenchmarkReport {
    std::string corruptionCsvPath;
    std::string storageCsvPath;
    std::string reportPath;
    std::string chartPath;
    std::size_t trialsPerMode = 0;
};

// Runs deterministic integrity-detection and deduplicated-storage experiments.
// Writes two CSV reports into outputDirectory. The synthetic experiment files
// are created in a unique temporary directory and removed afterwards.
bool runBenchmarks(const std::string& outputDirectory,
                   std::size_t trialsPerMode,
                   BenchmarkReport& report,
                   std::string& error);

} // namespace recoverx
