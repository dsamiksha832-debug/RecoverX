#include "recoverx/chunkstore.hpp"

#include "recoverx/atomicio.hpp"
#include "recoverx/sha256.hpp"

#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>

namespace recoverx {

namespace fs = std::filesystem;

ChunkStore::ChunkStore(std::string rootDirectory)
    : rootDirectory_(std::move(rootDirectory)) {}

bool ChunkStore::isValidContentId(const std::string& contentId) {
    if (contentId.size() != 64) {
        return false;
    }
    for (const char c : contentId) {
        const bool digit = (c >= '0' && c <= '9');
        const bool lowerHex = (c >= 'a' && c <= 'f');
        if (!digit && !lowerHex) {
            return false;
        }
    }
    return true;
}

std::string ChunkStore::chunkPath(const std::string& contentId) const {
    return (fs::path(rootDirectory_) / "chunks" / (contentId + ".chk")).string();
}

bool ChunkStore::has(const std::string& contentId) const {
    if (!isValidContentId(contentId) || index_.find(contentId) == index_.end()) {
        return false;
    }
    std::error_code ec;
    return fs::is_regular_file(chunkPath(contentId), ec);
}

bool ChunkStore::put(const std::vector<std::uint8_t>& bytes, std::string& contentId) {
    bool wasNew = false;
    return put(bytes, contentId, wasNew);
}

bool ChunkStore::put(const std::vector<std::uint8_t>& bytes,
                     std::string& contentId,
                     bool& wasNew) {
    wasNew = false;

    const std::string id = sha256Hex(bytes);
    if (!isValidContentId(id)) {
        return false; // the hash function broke its contract; refuse rather than write a bad name
    }

    if (has(id)) {
        contentId = id; // already stored: deduplicated, nothing written
        return true;
    }

    if (!atomicWriteFile(chunkPath(id), bytes)) {
        return false;
    }
    index_[id] = bytes.size();
    contentId = id;
    wasNew = true;
    return true;
}

bool ChunkStore::get(const std::string& contentId, std::vector<std::uint8_t>& bytes) const {
    if (!isValidContentId(contentId)) {
        return false;
    }
    return readFileBytes(chunkPath(contentId), bytes);
}

std::size_t ChunkStore::scan() {
    index_.clear();

    const fs::path dir = fs::path(rootDirectory_) / "chunks";
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        return 0;
    }

    removeStaleTempFiles(dir.string());

    fs::directory_iterator it(dir, ec);
    if (ec) {
        return 0;
    }
    fs::directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec) {
            break;
        }
        std::error_code entryEc;
        if (!it->is_regular_file(entryEc) || it->path().extension() != ".chk") {
            continue; // ignore anything that is not a chunk file
        }
        const std::string id = it->path().stem().string();
        if (!isValidContentId(id)) {
            continue;
        }
        const std::uintmax_t size = it->file_size(entryEc);
        index_[id] = entryEc ? 0 : static_cast<std::uint64_t>(size);
    }
    return index_.size();
}

bool ChunkStore::restoreToFile(const std::vector<std::string>& contentIds,
                               const std::string& outputPath) const {
    const std::string tmp = outputPath + ".tmp";
    std::error_code ec;

    const fs::path parent = fs::path(outputPath).parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent, ec);
    }

    bool ok = true;
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return false;
        }

        std::vector<std::uint8_t> bytes;
        for (const std::string& id : contentIds) {
            if (!get(id, bytes)) {
                ok = false; // missing or invalid chunk: stop, do not produce a partial file
                break;
            }
            if (!bytes.empty()) {
                out.write(reinterpret_cast<const char*>(bytes.data()),
                          static_cast<std::streamsize>(bytes.size()));
            }
            if (!out) {
                ok = false;
                break;
            }
        }
        out.flush();
        out.close();
        if (out.fail()) {
            ok = false;
        }
    }

    if (ok) {
        fs::rename(tmp, outputPath, ec);
        if (ec) {
            ok = false;
        }
    }
    if (!ok) {
        fs::remove(tmp, ec);
    }
    return ok;
}

} // namespace recoverx
