#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace recoverx {

// Content-addressed chunk storage. A chunk is saved as <root>/chunks/<sha256 hex>.chk, so
// identical chunks share one file (deduplication). An in-memory hash table (content id -> size)
// gives O(1) average lookup; the disk is the source of truth and scan() rebuilds the table.
//
// Known limitation (see PRD section 11): a chunk that is already stored is not re-checked, so
// if that stored chunk is later corrupted, every version that shares it is affected.
class ChunkStore {
public:
    explicit ChunkStore(std::string rootDirectory);

    // Stores `bytes` (if not already stored) and returns its content id (SHA-256 hex).
    // Returns false on any failure. The write is crash-safe (temp file + rename).
    bool put(const std::vector<std::uint8_t>& bytes, std::string& contentId);

    // Same, and `wasNew` tells whether a new chunk file was written (false = deduplicated).
    bool put(const std::vector<std::uint8_t>& bytes, std::string& contentId, bool& wasNew);

    // Reads a chunk. Returns false for an invalid id or a missing/unreadable file.
    // Does NOT verify the bytes; the caller (verify/recover) checks CRC-32 / SHA-256.
    bool get(const std::string& contentId, std::vector<std::uint8_t>& bytes) const;

    // True if the id is in the table AND its chunk file exists on disk.
    bool has(const std::string& contentId) const;

    // Startup scan: removes leftover *.tmp files from interrupted writes and rebuilds the
    // hash table from the chunk files on disk. Returns the number of chunks found.
    std::size_t scan();

    // Number of chunks currently in the hash table.
    std::size_t chunkCount() const { return index_.size(); }

    // Rebuilds a file by writing the listed chunks in order, one chunk in memory at a time.
    // Writes to outputPath + ".tmp" and renames at the end, so on failure (for example a missing
    // chunk) outputPath is untouched and no partial file remains. Returns false on failure.
    bool restoreToFile(const std::vector<std::string>& contentIds,
                       const std::string& outputPath) const;

    // Path of a chunk file. Only pass ids that passed isValidContentId().
    std::string chunkPath(const std::string& contentId) const;

    // A valid content id is exactly 64 lowercase hex characters. Checking this before touching
    // the disk stops ids like "../../x" from escaping the chunks folder.
    static bool isValidContentId(const std::string& contentId);

private:
    std::string rootDirectory_;
    std::unordered_map<std::string, std::uint64_t> index_;
};

} // namespace recoverx
