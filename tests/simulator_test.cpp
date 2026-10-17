
#include "recoverx/simulator.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

int main() {
    // Test bitflip.
    std::vector<std::uint8_t> a = {0x00, 0x01};
    assert(recoverx::corruptData(a, "bitflip"));
    assert(a[0] == 0x01);

    // Test burst.
    std::vector<std::uint8_t> b = {0x00, 0x01};
    assert(recoverx::corruptData(b, "burst"));
    assert(b[0] == 0x07);

    // Test swap.
    std::vector<std::uint8_t> c = {10, 20, 30};
    assert(recoverx::corruptData(c, "swap"));
    assert(c[0] == 30 && c[2] == 10);

    // Test truncate.
    std::vector<std::uint8_t> d = {1, 2, 3};
    assert(recoverx::corruptData(d, "truncate"));
    assert(d.size() == 2);

    // Test zero.
    std::vector<std::uint8_t> e = {1, 2, 3};
    assert(recoverx::corruptData(e, "zero"));
    assert(e[0] == 0 && e[1] == 0 && e[2] == 0);

    // Test deletechunk.
    std::vector<std::uint8_t> f = {1, 2, 3};
    assert(recoverx::corruptData(f, "deletechunk"));
    assert(f.empty());

    // Test invalid mode.
    std::vector<std::uint8_t> g = {1, 2, 3};
    assert(!recoverx::corruptData(g, "unknown"));
    assert(g.size() == 3);

    // Empty data must not be corrupted.
    std::vector<std::uint8_t> h;
    assert(!recoverx::corruptData(h, "bitflip"));

    return 0;
}
