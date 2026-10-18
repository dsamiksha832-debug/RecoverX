#include "recoverx/atomicio.hpp"

#include <filesystem>
#include <fstream>
#include <system_error>

namespace recoverx {

namespace fs = std::filesystem;

bool atomicWriteFile(const std::string& path, const std::vector<std::uint8_t>& bytes) {
    const std::string tmp = path + ".tmp";
    std::error_code ec;

    const fs::path parent = fs::path(path).parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent, ec); // ok if it already exists; failure shows up on open below
    }

    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    if (!bytes.empty()) {
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
    }
    out.flush();
    out.close();
    if (out.fail()) {
        fs::remove(tmp, ec);
        return false;
    }

    fs::rename(tmp, path, ec);
    if (ec) {
        fs::remove(tmp, ec);
        return false;
    }
    return true;
}

bool readFileBytes(const std::string& path, std::vector<std::uint8_t>& bytes) {
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) {
        return false;
    }

    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        return false;
    }
    const std::streamoff size = in.tellg();
    if (size < 0) {
        return false;
    }
    in.seekg(0);

    bytes.assign(static_cast<std::size_t>(size), 0);
    if (size > 0) {
        in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
        if (in.gcount() != static_cast<std::streamsize>(size)) {
            bytes.clear();
            return false;
        }
    }
    return true;
}

std::size_t removeStaleTempFiles(const std::string& directory) {
    std::error_code ec;
    std::vector<fs::path> stale;

    fs::directory_iterator it(directory, ec);
    if (ec) {
        return 0;
    }
    fs::directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec) {
            break;
        }
        std::error_code entryEc;
        if (it->is_regular_file(entryEc) && it->path().extension() == ".tmp") {
            stale.push_back(it->path());
        }
    }

    std::size_t removed = 0;
    for (const fs::path& p : stale) { // remove after iterating, not during
        std::error_code removeEc;
        if (fs::remove(p, removeEc)) {
            ++removed;
        }
    }
    return removed;
}

} // namespace recoverx
