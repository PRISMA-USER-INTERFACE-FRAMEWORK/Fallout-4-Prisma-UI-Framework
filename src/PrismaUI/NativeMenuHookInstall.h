#pragma once

#include <array>
#include <cstddef>

namespace PrismaUI::ControllerGlyphs {

inline constexpr int kNativeMenuHookOk = 0;

struct NativeMenuHookOps {
    int (*create)(void* target, void* detour, void** original);
    int (*enable)(void* target);
    int (*disable)(void* target);
    int (*remove)(void* target);
};

template <std::size_t N>
bool InstallNativeMenuHooks(const std::array<void*, N>& targets,
                            const std::array<void*, N>& detours,
                            const std::array<void**, N>& originals,
                            NativeMenuHookOps ops)
{
    if (!ops.create || !ops.enable || !ops.disable || !ops.remove) return false;
    std::size_t created = 0;
    for (; created < N; ++created) {
        if (ops.create(targets[created], detours[created], originals[created]) != kNativeMenuHookOk) {
            break;
        }
    }
    if (created == N) {
        std::size_t enabled = 0;
        for (; enabled < N; ++enabled) {
            if (ops.enable(targets[enabled]) != kNativeMenuHookOk) break;
        }
        if (enabled == N) return true;
    }
    for (std::size_t i = 0; i < created; ++i) {
        ops.disable(targets[i]);
        ops.remove(targets[i]);
    }
    return false;
}

}
