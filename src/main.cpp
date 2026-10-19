#include "recoverx/chunkstore.hpp"
#include "recoverx/benchmark.hpp"
#include "recoverx/checksum.hpp"
#include "recoverx/crc32.hpp"
#include "recoverx/manifest.hpp"
#include "recoverx/registration.hpp"
#include "recoverx/recovery.hpp"
#include "recoverx/simulator.hpp"
#include "recoverx/verification.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

constexpr const char* kDefaultStore = ".recoverx";
constexpr std::size_t kDefaultChunkSize = 4u * 1024u * 1024u;

void printUsage(std::ostream& out) {
    out << "RecoverX - versioned file backup and recovery\n\n"
        << "Usage:\n"
        << "  recoverx backup <file> [store-dir] [chunk-bytes]\n"
        << "  recoverx history <file> [store-dir]\n"
        << "  recoverx list <file> [store-dir]\n"
        << "  recoverx verify <version-id> [store-dir]\n"
        << "  recoverx verify <file> <version-id> [store-dir]\n"
        << "  recoverx restore <version-id> <output-file> [store-dir]\n"
        << "  recoverx recover <file> [store-dir]\n"
        << "  recoverx undo <file> [store-dir]\n"
        << "  recoverx corrupt <file> <version-id> <mode> [store-dir]\n"
        << "  recoverx checksum\n"
        << "  recoverx bench [output-dir] [trials-per-mode]\n"
        << "  recoverx help\n\n"
        << "Store directory defaults to .recoverx in the current directory.\n"
        << "Corruption modes: bitflip, burst, swap, truncate, zero, deletechunk.\n";
}

bool parseUnsigned(const std::string& text, unsigned long long& value) {
    if (text.empty()) return false;
    for (const unsigned char c : text) {
        if (!std::isdigit(c)) return false;
    }
    try {
        std::size_t consumed = 0;
        value = std::stoull(text, &consumed, 10);
        return consumed == text.size();
    } catch (...) {
        return false;
    }
}

int reportOperation(bool success, const std::string& successText,
                    const std::string& failureText) {
    if (!success) {
        std::cerr << "Error: " << failureText << '\n';
        return 1;
    }
    std::cout << successText << '\n';
    return 0;
}

bool loadVersion(const std::string& storeRoot,
                 unsigned long long versionId,
                 recoverx::VersionInfo& version) {
    const std::filesystem::path path = std::filesystem::path(storeRoot) / "manifests" /
        ("version_" + std::to_string(versionId) + ".rxm");
    return recoverx::readManifest(path.string(), version) && version.versionId == versionId;
}

int printHistory(const std::string& fileName, const std::string& storeRoot) {
    namespace fs = std::filesystem;
    const fs::path directory = fs::path(storeRoot) / "manifests";
    std::error_code ec;
    if (!fs::is_directory(directory, ec) || ec) {
        return reportOperation(false, {}, "no version history found for this file");
    }

    std::vector<recoverx::VersionInfo> versions;
    fs::directory_iterator it(directory, ec);
    if (ec) return reportOperation(false, {}, "cannot read the manifest directory");
    const fs::directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec) return reportOperation(false, {}, "cannot read the manifest directory");
        std::error_code entryEc;
        if (!it->is_regular_file(entryEc) || it->path().extension() != ".rxm") continue;
        recoverx::VersionInfo version;
        if (recoverx::readManifest(it->path().string(), version) &&
            version.originalFileName == fileName) {
            versions.push_back(std::move(version));
        }
    }
    if (versions.empty()) return reportOperation(false, {}, "no valid manifests found for this file");

    std::stable_sort(versions.begin(), versions.end(),
        [](const recoverx::VersionInfo& a, const recoverx::VersionInfo& b) {
            return a.versionId > b.versionId;
        });
    recoverx::ChunkStore store(storeRoot);
    store.scan();
    std::cout << "ID\tSIZE\tSTATUS\tCHUNKS\n";
    for (recoverx::VersionInfo& version : versions) {
        recoverx::verifyVersion(version, store);
        std::cout << version.versionId << '\t' << version.originalSize << '\t'
                  << recoverx::toString(version.status) << '\t'
                  << version.chunks.size() << '\n';
    }
    return 0;
}

int run(int argc, char** argv) {
    if (argc < 2 || std::string(argv[1]) == "help" || std::string(argv[1]) == "--help" ||
        std::string(argv[1]) == "-h") {
        printUsage(std::cout);
        return 0;
    }

    const std::string command = argv[1];
    if (command == "checksum") {
        const std::vector<std::uint8_t> sample{'R', 'e', 'c', 'o', 'v', 'e', 'r', 'X'};
        std::cout << "CRC-32: 0x" << std::hex << recoverx::crc32(sample) << '\n'
                  << "Internet checksum: 0x" << recoverx::internetChecksum(sample)
                  << std::dec << '\n';
        return 0;
    }

    if (command == "bench") {
        if (argc < 2 || argc > 4) {
            printUsage(std::cerr);
            return 2;
        }
        const std::string outputDirectory = argc >= 3
            ? argv[2] : std::string(kDefaultStore) + "/bench";
        std::size_t trialsPerMode = 1000;
        if (argc == 4) {
            unsigned long long parsed = 0;
            if (!parseUnsigned(argv[3], parsed) || parsed == 0 ||
                parsed > std::numeric_limits<std::size_t>::max()) {
                return reportOperation(false, {}, "trial count must be a positive integer");
            }
            trialsPerMode = static_cast<std::size_t>(parsed);
        }
        recoverx::BenchmarkReport report;
        std::string error;
        if (!recoverx::runBenchmarks(outputDirectory, trialsPerMode, report, error)) {
            return reportOperation(false, {}, error);
        }
        std::cout << "Benchmark complete (" << report.trialsPerMode
                  << " trials per corruption mode). Reports:\n  "
                  << report.corruptionCsvPath << "\n  " << report.storageCsvPath
                  << "\n  " << report.reportPath << "\n  " << report.chartPath << '\n';
        return 0;
    }
    if (command == "backup") {
        if (argc < 3 || argc > 5) {
            printUsage(std::cerr);
            return 2;
        }
        const std::string storeRoot = argc >= 4 ? argv[3] : kDefaultStore;
        std::size_t chunkSize = kDefaultChunkSize;
        if (argc == 5) {
            unsigned long long parsed = 0;
            if (!parseUnsigned(argv[4], parsed) || parsed == 0 ||
                parsed > std::numeric_limits<std::size_t>::max()) {
                return reportOperation(false, {}, "chunk size must be a positive integer");
            }
            chunkSize = static_cast<std::size_t>(parsed);
        }
        recoverx::VersionInfo version;
        if (!recoverx::registerVersion(argv[2], storeRoot, chunkSize, version)) {
            return reportOperation(false, {}, "backup failed; check the source and store paths");
        }
        std::cout << "Backed up " << version.originalFileName << " as version "
                  << version.versionId << " (" << version.originalSize << " bytes, "
                  << version.chunks.size() << " chunks) in " << storeRoot << '\n';
        return 0;
    }

    if (command == "history" || command == "list") {
        if (argc < 3 || argc > 4) {
            printUsage(std::cerr);
            return 2;
        }
        return printHistory(argv[2], argc == 4 ? argv[3] : kDefaultStore);
    }

    if (command == "verify") {
        if (argc < 3 || argc > 5) {
            printUsage(std::cerr);
            return 2;
        }
        std::string fileName;
        unsigned long long id = 0;
        std::string storeRoot = kDefaultStore;
        unsigned long long firstArgument = 0;
        if (parseUnsigned(argv[2], firstArgument)) {
            id = firstArgument;
            if (argc == 4) storeRoot = argv[3];
        } else {
            if (argc < 4 || !parseUnsigned(argv[3], id)) {
                return reportOperation(false, {}, "expected a positive version ID");
            }
            fileName = argv[2];
            if (argc == 5) storeRoot = argv[4];
        }
        if (id == 0) return reportOperation(false, {}, "version ID must be a positive integer");
        recoverx::VersionInfo version;
        if (!loadVersion(storeRoot, id, version)) {
            return reportOperation(false, {}, "manifest is missing or invalid");
        }
        if (!fileName.empty() && version.originalFileName != fileName) {
            return reportOperation(false, {}, "version does not belong to the requested file");
        }
        recoverx::ChunkStore store(storeRoot);
        store.scan();
        const bool valid = recoverx::verifyVersion(version, store);
        std::cout << "Version " << id << ": " << recoverx::toString(version.status) << '\n';
        return valid ? 0 : 1;
    }

    if (command == "restore") {
        if (argc < 4 || argc > 5) {
            printUsage(std::cerr);
            return 2;
        }
        unsigned long long id = 0;
        if (!parseUnsigned(argv[2], id) || id == 0) {
            return reportOperation(false, {}, "version ID must be a positive integer");
        }
        const std::string storeRoot = argc == 5 ? argv[4] : kDefaultStore;
        return reportOperation(recoverx::restoreVersion(id, storeRoot, argv[3]),
            "Restored version " + std::to_string(id) + " to " + argv[3],
            "restore failed; the version may be missing or corrupt");
    }

    if (command == "recover") {
        if (argc < 3 || argc > 4) {
            printUsage(std::cerr);
            return 2;
        }
        const std::string storeRoot = argc == 4 ? argv[3] : kDefaultStore;
        return reportOperation(recoverx::recoverLatestValidVersion(argv[2], storeRoot),
            "Recovered the newest valid version to " + std::string(argv[2]),
            "no valid version could be recovered");
    }

    if (command == "undo") {
        if (argc < 3 || argc > 4) {
            printUsage(std::cerr);
            return 2;
        }
        const std::string storeRoot = argc == 4 ? argv[3] : kDefaultStore;
        return reportOperation(recoverx::undoLastChange(argv[2], storeRoot),
            "Restored the previous valid version to " + std::string(argv[2]),
            "undo failed; an earlier valid version may not exist");
    }

    if (command == "corrupt") {
        if (argc < 5 || argc > 6) {
            printUsage(std::cerr);
            return 2;
        }
        unsigned long long id = 0;
        if (!parseUnsigned(argv[3], id) || id == 0) {
            return reportOperation(false, {}, "version ID must be a positive integer");
        }
        const std::string storeRoot = argc == 6 ? argv[5] : kDefaultStore;
        return reportOperation(recoverx::corruptStoredVersion(argv[2], storeRoot, id, argv[4]),
            "Applied " + std::string(argv[4]) + " corruption to version " + std::to_string(id),
            "corruption failed; the mode may be invalid or all chunks are shared");
    }

    printUsage(std::cerr);
    return reportOperation(false, {}, "unknown command: " + command);
}

} // namespace

int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
