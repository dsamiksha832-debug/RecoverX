#include "recoverx/chunkstore.hpp"
#include "recoverx/benchmark.hpp"
#include "recoverx/checksum.hpp"
#include "recoverx/crc32.hpp"
#include "recoverx/filehash.hpp"
#include "recoverx/manifest.hpp"
#include "recoverx/registration.hpp"
#include "recoverx/recovery.hpp"
#include "recoverx/sha256.hpp"
#include "recoverx/simulator.hpp"
#include "recoverx/verification.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

constexpr const char* kDefaultStore = ".recoverx";
constexpr std::size_t kDefaultChunkSize = 4096; // PRD default: 4 KB

void printUsage(std::ostream& out) {
    out << "RecoverX - versioned file backup and recovery\n\n"
        << "Usage:\n"
        << "  recoverx backup <file> [store-dir] [chunk-bytes]\n"
        << "  recoverx list <file> [store-dir]          (history is the same command)\n"
        << "  recoverx verify <version-id> [store-dir]\n"
        << "  recoverx verify <file> <version-id> [store-dir]\n"
        << "  recoverx recover <file> [store-dir] [--to <version-id>]\n"
        << "  recoverx restore <version-id> <output-file> [store-dir]\n"
        << "  recoverx undo <file> [store-dir]\n"
        << "  recoverx corrupt <file> <version-id> <mode> [store-dir]\n"
        << "  recoverx hash <file>\n"
        << "  recoverx checksum\n"
        << "  recoverx bench [output-dir] [trials-per-mode]\n"
        << "  recoverx help\n\n"
        << "Store directory defaults to .recoverx in the current directory.\n"
        << "Use the same file path for backup, list, verify, recover and undo.\n"
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

// Accepts "3", "V3" or "v3".
bool parseVersionId(std::string text, unsigned long long& value) {
    if (!text.empty() && (text[0] == 'V' || text[0] == 'v')) text.erase(0, 1);
    return parseUnsigned(text, value) && value != 0;
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

std::string manifestPathFor(const std::string& storeRoot, unsigned long long versionId) {
    return (std::filesystem::path(storeRoot) / "manifests" /
            ("version_" + std::to_string(versionId) + ".rxm")).string();
}

bool loadVersion(const std::string& storeRoot,
                 unsigned long long versionId,
                 recoverx::VersionInfo& version) {
    return recoverx::readManifest(manifestPathFor(storeRoot, versionId), version) &&
           version.versionId == versionId;
}

// Manifests do not store a timestamp, so the time a version was backed up is the time its
// manifest file was written.
std::string backupTime(const std::string& storeRoot, unsigned long long versionId) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const auto fileTime = fs::last_write_time(manifestPathFor(storeRoot, versionId), ec);
    if (ec) return "unknown";
    const auto systemTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        fileTime - decltype(fileTime)::clock::now() + std::chrono::system_clock::now());
    const std::time_t t = std::chrono::system_clock::to_time_t(systemTime);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    char text[32] = {};
    if (std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S", &local) == 0) return "unknown";
    return text;
}

std::string formatSeconds(double seconds) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(3) << seconds << " s";
    return out.str();
}

// "CRC-32 mismatch, SHA-256 mismatch" and so on: which check caught the damage.
std::string describeFailure(const recoverx::ChunkFailure& f) {
    std::vector<std::string> parts;
    if (f.invalidId) parts.push_back("malformed chunk id in manifest");
    if (f.missing) parts.push_back("chunk file missing");
    if (f.sizeMismatch) parts.push_back("size mismatch");
    if (f.crc32Mismatch) parts.push_back("CRC-32 mismatch");
    if (f.sha256Mismatch) parts.push_back("SHA-256 mismatch");
    std::string text;
    for (std::size_t i = 0; i < parts.size(); ++i) text += (i ? ", " : "") + parts[i];
    return text;
}

void printFailures(const recoverx::VerifyReport& report, const std::string& indent) {
    const std::size_t shown = std::min<std::size_t>(report.failures.size(), 10);
    for (std::size_t i = 0; i < shown; ++i) {
        const recoverx::ChunkFailure& f = report.failures[i];
        std::cout << indent << "chunk " << (f.index + 1) << " of " << report.chunkCount << ": "
                  << describeFailure(f) << '\n';
    }
    if (report.failures.size() > shown) {
        std::cout << indent << "... and " << (report.failures.size() - shown) << " more failed chunks\n";
    }
    if (report.totalSizeMismatch) {
        std::cout << indent << "chunk sizes do not add up to the recorded file size\n";
    }
}

void printDamagedManifests(const std::vector<std::uint64_t>& damaged) {
    for (const std::uint64_t id : damaged) {
        std::cout << "Warning: the manifest of version " << id
                  << " is damaged (its own CRC-32 failed). It was skipped and no chunk was blamed.\n";
    }
}

int printHistory(const std::string& fileName, const std::string& storeRoot) {
    std::vector<recoverx::VersionInfo> versions;
    std::vector<std::uint64_t> damaged;
    if (!recoverx::listVersions(fileName, storeRoot, versions, damaged)) {
        return reportOperation(false, {}, "no backup store found at " + storeRoot);
    }
    if (versions.empty() && damaged.empty()) {
        return reportOperation(false, {}, "no backup versions found for " + fileName);
    }

    recoverx::ChunkStore store(storeRoot);
    store.scan();
    std::cout << "ID\tTIME\t\t\tSIZE\tSTATUS\tCHUNKS\n";
    for (recoverx::VersionInfo& version : versions) {
        recoverx::verifyVersion(version, store);
        std::cout << version.versionId << '\t' << backupTime(storeRoot, version.versionId) << '\t'
                  << version.originalSize << '\t' << recoverx::toString(version.status) << '\t'
                  << version.chunks.size() << '\n';
    }
    printDamagedManifests(damaged);
    return 0;
}

int printRecoveryReport(const std::string& target, const recoverx::RecoveryReport& report) {
    for (const recoverx::VerifyReport& attempt : report.attempts) {
        std::cout << "  Version " << attempt.versionId << ": " << recoverx::toString(attempt.status)
                  << " (" << formatSeconds(attempt.seconds) << ")\n";
        printFailures(attempt, "      ");
    }
    printDamagedManifests(report.damagedManifests);
    if (!report.success) {
        std::cerr << "Error: " << report.error << '\n';
        return 1;
    }
    std::cout << "Restored version " << report.restoredVersionId << " to " << target << " ("
              << report.restoredBytes << " bytes)\n"
              << "  SHA-256 of restored file: " << report.restoredSha256 << '\n';
    if (report.undoSaved) {
        std::cout << "  The previous contents were saved on the undo stack (entry " << report.undoId
                  << "); run \"recoverx undo " << target << "\" to bring them back.\n";
    }
    std::cout << "  Total time: " << formatSeconds(report.seconds) << '\n';
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
        std::cout << "Sample data: RecoverX (8 bytes)\n"
                  << "CRC-32: 0x" << std::hex << std::setw(8) << std::setfill('0')
                  << recoverx::crc32(sample) << '\n'
                  << "Internet checksum: 0x" << std::setw(4) << recoverx::internetChecksum(sample)
                  << std::dec << std::setfill(' ') << '\n'
                  << "SHA-256: " << recoverx::sha256Hex(sample) << '\n'
                  << "To hash a real file, use: recoverx hash <file>\n";
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
            if (argc < 4 || !parseVersionId(argv[3], id)) {
                return reportOperation(false, {}, "expected a positive version ID");
            }
            fileName = argv[2];
            if (argc == 5) storeRoot = argv[4];
        }
        if (id == 0) return reportOperation(false, {}, "version ID must be a positive integer");

        recoverx::VersionInfo version;
        if (!loadVersion(storeRoot, id, version)) {
            std::error_code ec;
            if (std::filesystem::exists(manifestPathFor(storeRoot, id), ec)) {
                std::cout << "Version " << id << ": MANIFEST DAMAGED\n"
                          << "  The manifest's own CRC-32 failed, so it is not trusted and no chunk is blamed.\n";
            } else {
                std::cerr << "Error: version " << id << " does not exist in " << storeRoot << '\n';
            }
            return 1;
        }
        if (!fileName.empty() && !recoverx::sameFilePath(version.originalFileName, fileName)) {
            return reportOperation(false, {}, "version does not belong to the requested file");
        }

        recoverx::ChunkStore store(storeRoot);
        store.scan();
        recoverx::VerifyReport report;
        const bool valid = recoverx::verifyVersion(version, store, report);
        std::cout << "Version " << id << " of " << version.originalFileName << ": "
                  << recoverx::toString(version.status) << '\n'
                  << "  Manifest CRC-32: OK\n"
                  << "  Chunks checked: " << report.chunkCount << ", failed: " << report.failures.size() << '\n';
        printFailures(report, "    ");
        std::cout << "  Time: " << formatSeconds(report.seconds) << '\n';
        return valid ? 0 : 1;
    }

    if (command == "restore") {
        if (argc < 4 || argc > 5) {
            printUsage(std::cerr);
            return 2;
        }
        unsigned long long id = 0;
        if (!parseVersionId(argv[2], id)) {
            return reportOperation(false, {}, "version ID must be a positive integer");
        }
        const std::string storeRoot = argc == 5 ? argv[4] : kDefaultStore;
        recoverx::RecoveryReport report;
        recoverx::restoreVersion(id, storeRoot, argv[3], report);
        return printRecoveryReport(argv[3], report);
    }

    if (command == "recover") {
        std::vector<std::string> positional;
        unsigned long long target = 0;
        for (int i = 2; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--to") {
                if (i + 1 >= argc || !parseVersionId(argv[i + 1], target)) {
                    return reportOperation(false, {}, "--to needs a version ID such as 3 or V3");
                }
                ++i;
            } else {
                positional.push_back(arg);
            }
        }
        if (positional.empty() || positional.size() > 2) {
            printUsage(std::cerr);
            return 2;
        }
        const std::string& fileName = positional[0];
        const std::string storeRoot = positional.size() == 2 ? positional[1] : kDefaultStore;
        std::cout << "Recovering " << fileName;
        if (target != 0) std::cout << " (version " << target << " only)";
        std::cout << '\n';
        recoverx::RecoveryReport report;
        recoverx::recoverFile(fileName, storeRoot, report, target);
        return printRecoveryReport(fileName, report);
    }

    if (command == "undo") {
        if (argc < 3 || argc > 4) {
            printUsage(std::cerr);
            return 2;
        }
        const std::string storeRoot = argc == 4 ? argv[3] : kDefaultStore;
        recoverx::UndoReport report;
        if (!recoverx::undoRestore(argv[2], storeRoot, report)) {
            std::cerr << "Error: " << report.error << '\n';
            return 1;
        }
        std::cout << "Undo complete: " << argv[2] << " is back to its earlier state ("
                  << report.restoredBytes << " bytes)\n"
                  << "  SHA-256 of restored file: " << report.restoredSha256 << '\n'
                  << "  Undo entries left for this file: " << report.remaining << '\n';
        return 0;
    }

    if (command == "hash") {
        if (argc != 3) {
            printUsage(std::cerr);
            return 2;
        }
        recoverx::FileDigest digest;
        if (!recoverx::digestFile(argv[2], digest)) {
            return reportOperation(false, {}, "cannot read " + std::string(argv[2]));
        }
        std::cout << "File: " << argv[2] << '\n'
                  << "Size: " << digest.size << " bytes\n"
                  << "CRC-32: 0x" << std::hex << std::setw(8) << std::setfill('0') << digest.crc32
                  << '\n'
                  << "Internet checksum: 0x" << std::setw(4) << digest.internetChecksum << std::dec
                  << std::setfill(' ') << '\n'
                  << "SHA-256: " << digest.sha256 << '\n';
        return 0;
    }

    if (command == "corrupt") {
        if (argc < 5 || argc > 6) {
            printUsage(std::cerr);
            return 2;
        }
        unsigned long long id = 0;
        if (!parseVersionId(argv[3], id)) {
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
