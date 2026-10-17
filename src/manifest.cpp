#include "recoverx/manifest.hpp"

namespace recoverx {

bool writeManifest(const std::string& /*path*/, const VersionInfo& /*version*/) {
    // TODO: define a versioned manifest format; protect its contents with CRC-32.
    return false;
}

bool readManifest(const std::string& /*path*/, VersionInfo& /*version*/) {
    // TODO: parse defensively and reject malformed or CRC-invalid manifests.
    return false;
}

} // namespace recoverx
