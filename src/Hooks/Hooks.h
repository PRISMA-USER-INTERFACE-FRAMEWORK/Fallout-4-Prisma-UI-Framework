#pragma once

#include <MinHook.h>
#include <REX/W32/DXGI.h>

#include <atomic>
#include <cstdint>
#include <mutex>

#include "Hooks/ReattachPolicy.h"

namespace Hooks {
    struct D3DHooks {
        using PresentFunc = HRESULT(APIENTRY*)(REX::W32::IDXGISwapChain*, UINT, UINT);
        using ResizeBuffersFunc = HRESULT(APIENTRY*)(REX::W32::IDXGISwapChain*, UINT, UINT, UINT,
                                                      REX::W32::DXGI_FORMAT, UINT);

        static inline std::atomic<PresentFunc> s_presentTrampolineA{nullptr};
        static inline std::atomic<PresentFunc> s_presentTrampolineB{nullptr};
        static inline std::atomic<ResizeBuffersFunc> s_resizeTrampolineA{nullptr};
        static inline std::atomic<ResizeBuffersFunc> s_resizeTrampolineB{nullptr};

        static inline std::atomic<void*> s_presentTargetA{nullptr};
        static inline std::atomic<void*> s_presentTargetB{nullptr};
        static inline std::atomic<void*> s_resizeTargetA{nullptr};
        static inline std::atomic<void*> s_resizeTargetB{nullptr};
        static inline std::atomic<int> s_presentInFlightA{0};
        static inline std::atomic<int> s_presentInFlightB{0};
        static inline std::atomic<int> s_resizeInFlightA{0};
        static inline std::atomic<int> s_resizeInFlightB{0};
        static inline std::atomic<int> s_activePresentSlot{0};
        static inline std::atomic<int> s_activeResizeSlot{0};

        static inline std::atomic<void*> s_hookedPresentTarget{nullptr};
        static inline std::atomic<void*> s_hookedResizeTarget{nullptr};
        static inline std::atomic<uint32_t> s_generation{0};
        static inline std::atomic<int64_t> s_lastReattachAttemptMs{0};
        static inline std::atomic<int64_t> s_lastReattachMs{0};
        static inline std::atomic<bool> s_presentOrphanedA{false};
        static inline std::atomic<bool> s_presentOrphanedB{false};
        static inline std::atomic<bool> s_resizeOrphanedA{false};
        static inline std::atomic<bool> s_resizeOrphanedB{false};
        static inline std::atomic<bool> s_minHookOrphaned{false};
        static inline unsigned char s_presentPatchA[24]{};
        static inline unsigned char s_presentPatchB[24]{};
        static inline unsigned char s_resizePatchA[24]{};
        static inline unsigned char s_resizePatchB[24]{};
        static inline std::mutex s_hookLifecycleMutex;

        static HRESULT APIENTRY HookPresentA(REX::W32::IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
        static HRESULT APIENTRY HookPresentB(REX::W32::IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
        static HRESULT APIENTRY HookResizeBuffersA(REX::W32::IDXGISwapChain* pSwapChain, UINT BufferCount,
                                                   UINT Width, UINT Height, REX::W32::DXGI_FORMAT NewFormat,
                                                   UINT SwapChainFlags);
        static HRESULT APIENTRY HookResizeBuffersB(REX::W32::IDXGISwapChain* pSwapChain, UINT BufferCount,
                                                   UINT Width, UINT Height, REX::W32::DXGI_FORMAT NewFormat,
                                                   UINT SwapChainFlags);

        static void Install();

        static bool Reattach(void* newPresentTarget, void* newResizeTarget, int64_t nowMs,
                             uint32_t expectedGeneration);
        static bool OwnsActiveSwapchainChain(REX::W32::IDXGISwapChain* swapChain);
        static PrismaUI::Hooks::HookCoverage CaptureCoverage();
        static PrismaUI::Hooks::HookCoverage CurrentCoverage();
        static void Uninstall();
    };
}
