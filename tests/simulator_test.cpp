
#include "recoverx/simulator.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

int main() {
    // Test bitflip.
    std::vector<std::uint8_t> a = {0x00, 0x01, 0x02};
    assert(recoverx::corruptData(a, "bitflip"));
    assert(a[0] == 0x01);
    assert(a[1] == 0x01 && a[2] == 0x02);

    // Test burst.
    std::vector<std::uint8_t> b = {0x00, 0x01, 0x02};
    assert(recoverx::corruptData(b, "burst"));
    assert(b[0] == 0x07);
    assert(b[1] == 0x01 && b[2] == 0x02);

    // Test swap.
    std::vector<std::uint8_t> c = {10, 20, 30, 40};
    assert(recoverx::corruptData(c, "swap"));
    assert(c[0] == 40 && c[3] == 10);
    assert(c[1] == 20 && c[2] == 30);

    // Swap must fail for a single byte.
    std::vector<std::uint8_t> d = {50};
    assert(!recoverx::corruptData(d, "swap"));
    assert(d.size() == 1 && d[0] == 50);

    // Test truncate.
    std::vector<std::uint8_t> e = {1, 2, 3};
    assert(recoverx::corruptData(e, "truncate"));
    assert(e.size() == 2);
    assert(e[0] == 1 && e[1] == 2);

    // Truncating one byte should produce empty data.
    std::vector<std::uint8_t> f = {99};
    assert(recoverx::corruptData(f, "truncate"));
    assert(f.empty());

    // Test zero.
    std::vector<std::uint8_t> g = {1, 2, 3};
    assert(recoverx::corruptData(g, "zero"));
    assert(g.size() == 3);
    assert(g[0] == 0 && g[1] == 0 && g[2] == 0);

    // Test deletechunk.
    std::vector<std::uint8_t> h = {1, 2, 3};
    assert(recoverx::corruptData(h, "deletechunk"));
    assert(h.empty());

    // Test invalid mode.
    std::vector<std::uint8_t> i = {1, 2, 3};
    assert(!recoverx::corruptData(i, "unknown"));
    assert(i.size() == 3);
    assert(i[0] == 1 && i[1] == 2 && i[2] == 3);

    // Empty data must not be corrupted.
    std::vector<std::uint8_t> j;
    assert(!recoverx::corruptData(j, "bitflip"));
    assert(j.empty());

    return 0;
}
