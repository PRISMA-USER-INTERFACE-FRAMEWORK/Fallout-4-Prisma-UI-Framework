#include "PrismaUI/ControllerKeyState.h"

#include <cassert>
#include <vector>

int main() {
    PrismaUI::Web::ControllerKeyState keys;
    assert(!keys.Apply(0, true, true));
    assert(!keys.Apply(256, true, true));
    assert(!keys.Apply(13, true, false));
    assert(!keys.Apply(13, false, false));
    assert(keys.Apply(13, true, true));
    assert(!keys.Apply(13, true, true));
    assert(!keys.Apply(13, true, true));
    for (int repeat = 0; repeat < 1000; ++repeat) assert(keys.Apply(13, true, false));
    assert(keys.Apply(13, false, false));
    assert(!keys.Apply(13, false, false));
    assert(keys.Apply(38, true, true));
    assert(keys.Apply(38, false, false));
    assert(keys.Apply(39, true, true));
    assert(keys.Apply(39, true, false));
    const auto releasedOnHandoff = keys.down;
    keys = {};
    assert(releasedOnHandoff[39]);
    assert(!keys.Apply(38, true, false));
    assert(!keys.Apply(38, false, false));
    assert(keys.Apply(38, true, true));
    std::vector<std::uint32_t> releases;
    keys.Release([&](std::uint32_t key) {
        assert(keys.down.none());
        releases.push_back(key);
        return true;
    });
    assert((releases == std::vector<std::uint32_t>{38}));
    assert(keys.Apply(13, true, true));
    keys = {};
    keys.Release([](std::uint32_t) { assert(false); return true; });
    assert(keys.Apply(13, true, true));
    assert(keys.Apply(38, true, true));
    std::uint64_t documentGeneration = 1;
    releases.clear();
    keys.Release([&](std::uint32_t key) {
        if (documentGeneration != 1) return false;
        releases.push_back(key);
        ++documentGeneration;
        assert(keys.Apply(39, true, true));
        return true;
    });
    assert((releases == std::vector<std::uint32_t>{13}));
    assert(keys.down.count() == 1 && keys.down[39]);
}
