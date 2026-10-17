#include "recoverx/simulator.hpp"

namespace recoverx {

bool corruptStoredVersion(const std::string& /*fileName*/,
                          unsigned long long /*versionId*/,
                          const std::string& /*mode*/) {
    // TODO: only corrupt test-store data, never the user's source file.
    return false;
}

} // namespace recoverx
