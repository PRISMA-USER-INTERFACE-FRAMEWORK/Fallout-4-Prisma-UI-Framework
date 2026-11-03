#include "WebRuntimeInternal.h"
#include "GameThreadDispatcher.h"
#include "Engine/EngineBackbuffer.h"
#include "RuntimeStartupPolicy.h"
#include <chrono>
#include <thread>

namespace PrismaUI::WebRuntime {
    namespace { RuntimeStartupPolicy::Request request; }

    std::uint32_t GetRuntimeState() noexcept {
        auto& state = Internal::RT();
        return static_cast<std::uint32_t>(RuntimeStartupPolicy::State(
            state.initFailed.load(std::memory_order_acquire), state.active.load(std::memory_order_acquire),
            GameThreadDispatcher::IsReady(), request.Claimed()));
    }

    void RequestStartup(bool force) noexcept {
        auto* renderer = RE::BSGraphics::GetRendererData();
        auto* window = ::PrismaUI::Engine::MainRendererWindow();
        const bool eligible = force || (renderer && renderer->device && renderer->context && window && window->hwnd &&
            window->swapChain);
        if (!request.Claim(eligible)) return;
        try {
            std::thread([] {
                const auto start = std::chrono::steady_clock::now();
                logger::info("[Startup] Ultralight backend worker entered");
                try {
                    if (!LoadAndInit()) Internal::RT().initFailed.store(true, std::memory_order_release);
                } catch (...) {
                    Internal::RT().initFailed.store(true, std::memory_order_release);
                }
                logger::info("[Startup] backend request finished elapsedMs={} state={}",
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count(),
                    GetRuntimeState());
            }).detach();
        } catch (...) {
            Internal::RT().initFailed.store(true, std::memory_order_release);
        }
    }
}

extern "C" __declspec(dllexport) std::uint32_t PrismaUI_F4_GetRuntimeState() noexcept {
    return PrismaUI::WebRuntime::GetRuntimeState();
}
