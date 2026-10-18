#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace recoverx {

// Crash-safe file write: writes path + ".tmp", flushes, closes, then renames it over `path`.
// A rename inside one folder is a single step, so `path` is either the old file or the
// complete new file, never a half-written one. Parent folders are created if missing.
// Returns false on any failure; the destination is then left unchanged and the temp file removed.
// Note: flush/close hand data to the OS; surviving a sudden power cut would also need fsync
// (OS specific) - listed as future work in the PRD.
bool atomicWriteFile(const std::string& path, const std::vector<std::uint8_t>& bytes);

// Reads a whole regular file in binary mode. Returns false if it is missing or unreadable.
// Intended for small files (chunks, manifests); use ChunkReader for large files.
bool readFileBytes(const std::string& path, std::vector<std::uint8_t>& bytes);

// Deletes leftover "*.tmp" files directly inside `directory` (left behind by an interrupted
// write). Returns how many were removed. Call at startup.
std::size_t removeStaleTempFiles(const std::string& directory);

} // namespace recoverx
