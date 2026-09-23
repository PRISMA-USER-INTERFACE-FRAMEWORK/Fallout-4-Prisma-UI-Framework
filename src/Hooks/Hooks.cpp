#include "Hooks.h"
#include "Hooks/InlineHookClassifier.h"
#include "PrismaUI/WebRuntime.h"
#include "Utils/ConflictChecker.h"

#include <chrono>
#include <thread>

namespace Hooks {

    #include "HooksInternals.inl"
        static_assert(sizeof(s_chainSlotCount) == 2 * sizeof(std::atomic<unsigned int>));
        bool PrologueOwnedBy(void* target, const unsigned char* expected) {
            if (!target || !expected) return false;
            const uintptr_t address = reinterpret_cast<uintptr_t>(target);
            if (address < kPatchPrefixBytes) return false;
            unsigned char bytes[kPatchWindowBytes]{};
            __try {
                std::memcpy(bytes, reinterpret_cast<const void*>(address - kPatchPrefixBytes), sizeof(bytes));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
            for (size_t i = 0; i < sizeof(bytes); ++i) {
                if (bytes[i] != expected[i]) return false;
            }
            return true;
        }

        bool CapturePatch(void* target, unsigned char* output) {
            if (!target || !output) return false;
            const uintptr_t address = reinterpret_cast<uintptr_t>(target);
            if (address < kPatchPrefixBytes) return false;
            __try {
                std::memcpy(output, reinterpret_cast<const void*>(address - kPatchPrefixBytes),
                            kPatchWindowBytes);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }

        uintptr_t ResolveDetour(uintptr_t address, int depth) {
            if (!address || depth > 4) return 0;
            unsigned char bytes[16]{};
            __try {
                std::memcpy(bytes, reinterpret_cast<const void*>(address), sizeof(bytes));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return 0;
            }
            if (bytes[0] == 0xE9) {
                int32_t displacement = 0;
                std::memcpy(&displacement, bytes + 1, sizeof(displacement));
                return ResolveDetour(address + 5 + static_cast<intptr_t>(displacement), depth + 1);
            }
            if (bytes[0] == 0xEB) {
                const auto displacement = static_cast<int8_t>(bytes[1]);
                return ResolveDetour(address + 2 + displacement, depth + 1);
            }
            if (bytes[0] == 0x48 && bytes[1] == 0xB8 && bytes[10] == 0xFF && bytes[11] == 0xE0) {
                uintptr_t destination = 0;
                std::memcpy(&destination, bytes + 2, sizeof(destination));
                return destination;
            }
            if (bytes[0] == 0xFF && bytes[1] == 0x25) {
                int32_t displacement = 0;
                std::memcpy(&displacement, bytes + 2, sizeof(displacement));
                uintptr_t slot = address + 6 + static_cast<intptr_t>(displacement);
                uintptr_t destination = 0;
                __try {
                    std::memcpy(&destination, reinterpret_cast<const void*>(slot), sizeof(destination));
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    return 0;
                }
                return destination;
            }
            return 0;
        }

        bool PatchReachesDetour(void* target, void* detour) {
            if (!target || !detour) return false;
            const uintptr_t address = reinterpret_cast<uintptr_t>(target);
            const uintptr_t destination = ResolveDetour(address, 0);
            if (destination == reinterpret_cast<uintptr_t>(detour)) return true;
            if (address < 5) return false;
            return ResolveDetour(address - 5, 0) == reinterpret_cast<uintptr_t>(detour);
        }

        RetirementResult RemoveRetiredSlot(void* target, const unsigned char* expected, std::atomic<int>& inFlight,
                                            const char* label, int slot) {
            if (!target) return RetirementResult::Removed;
            if (!PrologueOwnedBy(target, expected)) {
                logger::warn("D3DHooks: {} slot {} target {} is externally owned; leaving MinHook entry orphaned",
                             label, slot, target);
                D3DHooks::s_minHookOrphaned.store(true, std::memory_order_release);
                return RetirementResult::Orphaned;
            }

            const MH_STATUS disabled = MH_DisableHook(target);
            if (!IsDisableSuccess(disabled)) {
                logger::warn("D3DHooks: could not disable retired {} slot {} target {} (MinHook status {})",
                             label, slot, target, static_cast<int>(disabled));
                return RetirementResult::Deferred;
            }

            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
            while (inFlight.load(std::memory_order_acquire) != 0) {
                if (std::chrono::steady_clock::now() >= deadline) {
                    logger::warn("D3DHooks: retired {} slot {} still has an invocation in flight; deferring removal",
                                 label, slot);
                    return RetirementResult::Deferred;
                }
                std::this_thread::yield();
            }

            const MH_STATUS removed = MH_RemoveHook(target);
            if (removed != MH_OK && removed != MH_ERROR_NOT_CREATED) {
                logger::warn("D3DHooks: could not remove retired {} slot {} target {} (MinHook status {})",
                             label, slot, target, static_cast<int>(removed));
                return RetirementResult::Deferred;
            }
            return RetirementResult::Removed;
        }

        bool RemoveRetiredPresentSlot(int slot) {
            if (PresentOrphaned(slot).load(std::memory_order_acquire)) return false;
            void* target = PresentTarget(slot).load(std::memory_order_acquire);
            const auto result = RemoveRetiredSlot(target, PresentPatch(slot), PresentInFlight(slot), "Present", slot);
            if (result == RetirementResult::Deferred) return false;
            if (result == RetirementResult::Orphaned) {
                PresentOrphaned(slot).store(true, std::memory_order_release);
                return false;
            }
            PresentTarget(slot).store(nullptr, std::memory_order_release);
            PresentTrampoline(slot).store(nullptr, std::memory_order_release);
            return true;
        }

        void QuarantinePresentSlot(int slot, void* target, D3DHooks::PresentFunc trampoline) {
            PresentTrampoline(slot).store(trampoline, std::memory_order_release);
            PresentTarget(slot).store(target, std::memory_order_release);
            PresentOrphaned(slot).store(true, std::memory_order_release);
            D3DHooks::s_minHookOrphaned.store(true, std::memory_order_release);
        }

        void QuarantineResizeSlot(int slot, void* target, D3DHooks::ResizeBuffersFunc trampoline) {
            ResizeTrampoline(slot).store(trampoline, std::memory_order_release);
            ResizeTarget(slot).store(target, std::memory_order_release);
            ResizeOrphaned(slot).store(true, std::memory_order_release);
            D3DHooks::s_minHookOrphaned.store(true, std::memory_order_release);
        }

        bool RemoveRetiredResizeSlot(int slot) {
            if (ResizeOrphaned(slot).load(std::memory_order_acquire)) return false;
            void* target = ResizeTarget(slot).load(std::memory_order_acquire);
            const auto result = RemoveRetiredSlot(target, ResizePatch(slot), ResizeInFlight(slot), "ResizeBuffers", slot);
            if (result == RetirementResult::Deferred) return false;
            if (result == RetirementResult::Orphaned) {
                ResizeOrphaned(slot).store(true, std::memory_order_release);
                return false;
            }
            ResizeTarget(slot).store(nullptr, std::memory_order_release);
            ResizeTrampoline(slot).store(nullptr, std::memory_order_release);
            return true;
        }

        bool EnablePresentSlot(int slot, void* target) {
            if (PresentOrphaned(slot).load(std::memory_order_acquire)) return false;
            if (!PrismaUI::ConflictChecker::ClassifyHookTargetForInstall(target).safe) {
                logger::warn("D3DHooks: Present slot {} failed the install-time ownership policy", slot);
                return false;
            }
            D3DHooks::PresentFunc trampoline = nullptr;
            const MH_STATUS created = MH_CreateHook(target, PresentDetour(slot),
                                                    reinterpret_cast<LPVOID*>(&trampoline));
            if (created != MH_OK || !trampoline) {
                logger::warn("D3DHooks: MH_CreateHook(Present slot {}) failed ({})",
                             slot, static_cast<int>(created));
                if (created == MH_OK) MH_RemoveHook(target);
                return false;
            }
            PresentTrampoline(slot).store(trampoline, std::memory_order_release);
            const MH_STATUS enabled = MH_EnableHook(target);
            if (enabled != MH_OK) {
                logger::warn("D3DHooks: MH_EnableHook(Present slot {}) failed ({})",
                             slot, static_cast<int>(enabled));
                MH_RemoveHook(target);
                PresentTrampoline(slot).store(nullptr, std::memory_order_release);
                return false;
            }
            if (!PatchReachesDetour(target, PresentDetour(slot))) {
                logger::warn("D3DHooks: Present slot {} patch ownership was lost during enable; quarantining slot", slot);
                QuarantinePresentSlot(slot, target, trampoline);
                return false;
            }
            if (!CapturePatch(target, PresentPatch(slot))) {
                logger::warn("D3DHooks: Present slot {} patch capture failed; quarantining slot", slot);
                QuarantinePresentSlot(slot, target, trampoline);
                return false;
            }
            if (!PatchReachesDetour(target, PresentDetour(slot))) {
                logger::warn("D3DHooks: Present slot {} changed during ownership capture; quarantining slot", slot);
                QuarantinePresentSlot(slot, target, trampoline);
                return false;
            }
            PresentTarget(slot).store(target, std::memory_order_release);
            return true;
        }

        bool EnableResizeSlot(int slot, void* target) {
            if (ResizeOrphaned(slot).load(std::memory_order_acquire)) return false;
            if (!PrismaUI::ConflictChecker::ClassifyHookTargetForInstall(target).safe) {
                logger::warn("D3DHooks: ResizeBuffers slot {} failed the install-time ownership policy", slot);
                return false;
            }
            D3DHooks::ResizeBuffersFunc trampoline = nullptr;
            const MH_STATUS created = MH_CreateHook(target, ResizeDetour(slot),
                                                    reinterpret_cast<LPVOID*>(&trampoline));
            if (created != MH_OK || !trampoline) {
                logger::warn("D3DHooks: MH_CreateHook(ResizeBuffers slot {}) failed ({})",
                             slot, static_cast<int>(created));
                if (created == MH_OK) MH_RemoveHook(target);
                return false;
            }
            ResizeTrampoline(slot).store(trampoline, std::memory_order_release);
            const MH_STATUS enabled = MH_EnableHook(target);
            if (enabled != MH_OK) {
                logger::warn("D3DHooks: MH_EnableHook(ResizeBuffers slot {}) failed ({})",
                             slot, static_cast<int>(enabled));
                MH_RemoveHook(target);
                ResizeTrampoline(slot).store(nullptr, std::memory_order_release);
                return false;
            }
            if (!PatchReachesDetour(target, ResizeDetour(slot))) {
                logger::warn("D3DHooks: ResizeBuffers slot {} patch ownership was lost during enable; quarantining slot", slot);
                QuarantineResizeSlot(slot, target, trampoline);
                return false;
            }
            if (!CapturePatch(target, ResizePatch(slot))) {
                logger::warn("D3DHooks: ResizeBuffers slot {} patch capture failed; quarantining slot", slot);
                QuarantineResizeSlot(slot, target, trampoline);
                return false;
            }
            if (!PatchReachesDetour(target, ResizeDetour(slot))) {
                logger::warn("D3DHooks: ResizeBuffers slot {} changed during ownership capture; quarantining slot", slot);
                QuarantineResizeSlot(slot, target, trampoline);
                return false;
            }
            ResizeTarget(slot).store(target, std::memory_order_release);
            return true;
        }

        bool ChainOwnsDispatch(int slot, REX::W32::IDXGISwapChain* swapChain) {
            if (!swapChain || ChainSwapchain(slot).load(std::memory_order_acquire) != swapChain)
                return false;
            void** current = *reinterpret_cast<void***>(swapChain);
            void** shadow = ChainShadowVtable(slot).load(std::memory_order_acquire);
            return current == shadow && shadow && shadow[8] == PresentDetour(slot) &&
                   shadow[13] == ResizeDetour(slot);
        }

        bool DrainChainInFlight(int slot) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
            while (PresentInFlight(slot).load(std::memory_order_acquire) != 0 ||
                   ResizeInFlight(slot).load(std::memory_order_acquire) != 0) {
                if (std::chrono::steady_clock::now() >= deadline) return false;
                std::this_thread::yield();
            }
            return true;
        }

        void ReleaseChainSwapchain(int slot) {
            auto* swapChain = ChainSwapchain(slot).exchange(nullptr, std::memory_order_acq_rel);
            if (swapChain)
                swapChain->Release();
        }

        void FinalizeChainSlot(int slot) {
            SlotStrategy(slot).store(PrismaUI::Hooks::HookInstallStrategy::Reject,
                                     std::memory_order_release);
            ChainPreviousPresent(slot).store(nullptr, std::memory_order_release);
            ChainPreviousResize(slot).store(nullptr, std::memory_order_release);
            ReleaseChainSwapchain(slot);
            ChainOriginalVtable(slot).store(nullptr, std::memory_order_release);
            ChainShadowVtable(slot).store(nullptr, std::memory_order_release);
            ChainSlotCount(slot).store(0, std::memory_order_release);
            ChainOrphaned(slot).store(false, std::memory_order_release);
            s_chainState[slot].store(ChainRetirementState::Free, std::memory_order_release);
        }

        void PreserveChainRelay(int slot) {
            ChainOrphaned(slot).store(true, std::memory_order_release);
            ReleaseChainSwapchain(slot);
            ChainOriginalVtable(slot).store(nullptr, std::memory_order_release);
            ChainShadowVtable(slot).store(nullptr, std::memory_order_release);
            ChainSlotCount(slot).store(0, std::memory_order_release);
            s_chainState[slot].store(ChainRetirementState::ForwardRelay, std::memory_order_release);
        }

        bool InstallChainSlot(int slot, REX::W32::IDXGISwapChain* swapChain,
                              void* requiredPresent = nullptr, void* requiredResize = nullptr,
                              void* requiredDispatch = nullptr, unsigned int requiredShadowSlots = 0) {
            if (!swapChain || ChainOrphaned(slot).load(std::memory_order_acquire) ||
                s_chainState[slot].load(std::memory_order_acquire) != ChainRetirementState::Free)
                return false;
            if (requiredShadowSlots < kBaseSwapchainVtableSlots ||
                requiredShadowSlots > kMaxSwapchainVtableSlots)
                return false;
            void** original = *reinterpret_cast<void***>(swapChain);
            if (!original) return false;
            if (requiredDispatch && original != requiredDispatch) return false;
            void* expectedPresent = original[8];
            void* expectedResize = original[13];
            if (requiredPresent && expectedPresent != requiredPresent) return false;
            if (requiredResize && expectedResize != requiredResize) return false;
            void** shadow = ChainVtable(slot);
            std::memcpy(shadow, original, sizeof(void*) * requiredShadowSlots);
            shadow[8] = PresentDetour(slot);
            shadow[13] = ResizeDetour(slot);
            swapChain->AddRef();
            ChainSwapchain(slot).store(swapChain, std::memory_order_release);
            ChainOriginalVtable(slot).store(original, std::memory_order_release);
            ChainShadowVtable(slot).store(shadow, std::memory_order_release);
            ChainSlotCount(slot).store(requiredShadowSlots, std::memory_order_release);
            ChainPreviousPresent(slot).store(reinterpret_cast<D3DHooks::PresentFunc>(expectedPresent),
                                              std::memory_order_release);
            ChainPreviousResize(slot).store(reinterpret_cast<D3DHooks::ResizeBuffersFunc>(expectedResize),
                                             std::memory_order_release);
            SlotStrategy(slot).store(PrismaUI::Hooks::HookInstallStrategy::SwapchainChain,
                                      std::memory_order_release);
            s_chainState[slot].store(ChainRetirementState::Active, std::memory_order_release);
            void** current = *reinterpret_cast<void***>(swapChain);
            if (!current || current != original || current[8] != expectedPresent ||
                current[13] != expectedResize) {
                s_chainState[slot].store(ChainRetirementState::Free, std::memory_order_release);
                SlotStrategy(slot).store(PrismaUI::Hooks::HookInstallStrategy::Reject,
                                         std::memory_order_release);
                ChainPreviousPresent(slot).store(nullptr, std::memory_order_release);
                ChainPreviousResize(slot).store(nullptr, std::memory_order_release);
                swapChain->Release();
                ChainSwapchain(slot).store(nullptr, std::memory_order_release);
                ChainOriginalVtable(slot).store(nullptr, std::memory_order_release);
                ChainShadowVtable(slot).store(nullptr, std::memory_order_release);
                ChainSlotCount(slot).store(0, std::memory_order_release);
                return false;
            }
            const auto previous = InterlockedCompareExchangePointer(
                reinterpret_cast<PVOID volatile*>(swapChain), shadow, original);
            if (previous != original) {
                s_chainState[slot].store(ChainRetirementState::Free, std::memory_order_release);
                SlotStrategy(slot).store(PrismaUI::Hooks::HookInstallStrategy::Reject,
                                         std::memory_order_release);
                ChainPreviousPresent(slot).store(nullptr, std::memory_order_release);
                ChainPreviousResize(slot).store(nullptr, std::memory_order_release);
                swapChain->Release();
                ChainSwapchain(slot).store(nullptr, std::memory_order_release);
                ChainOriginalVtable(slot).store(nullptr, std::memory_order_release);
                ChainShadowVtable(slot).store(nullptr, std::memory_order_release);
                ChainSlotCount(slot).store(0, std::memory_order_release);
                return false;
            }
            return true;
        }

        bool RetireChainSlot(int slot) {
            const auto state = s_chainState[slot].load(std::memory_order_acquire);
            if (state == ChainRetirementState::Free) return true;
            if (!PrismaUI::Hooks::RelaySlotReusable(state == ChainRetirementState::ForwardRelay)) return false;
            if (state == ChainRetirementState::DetachedDraining) {
                if (!DrainChainInFlight(slot)) return false;
                FinalizeChainSlot(slot);
                return true;
            }
            void* shadowObject = ChainShadowVtable(slot).load(std::memory_order_acquire);
            auto* swapChain = ChainSwapchain(slot).load(std::memory_order_acquire);
            void** original = ChainOriginalVtable(slot).load(std::memory_order_acquire);
            if (!shadowObject || !swapChain || !original) return true;
            void** shadow = reinterpret_cast<void**>(shadowObject);
            void** current = *reinterpret_cast<void***>(swapChain);
            if (current != shadow || shadow[8] != PresentDetour(slot) || shadow[13] != ResizeDetour(slot)) {
                PreserveChainRelay(slot);
                logger::warn("D3DHooks: chain slot {} is no longer Prisma-owned; preserving its forward relay", slot);
                return false;
            }
            const auto previous = InterlockedCompareExchangePointer(
                reinterpret_cast<PVOID volatile*>(swapChain), original, shadow);
            if (previous != shadow) {
                PreserveChainRelay(slot);
                logger::warn("D3DHooks: chain slot {} ownership changed during retirement; preserving its forward relay",
                             slot);
                return false;
            }
            s_chainState[slot].store(ChainRetirementState::DetachedDraining, std::memory_order_release);
            if (!DrainChainInFlight(slot)) return false;
            FinalizeChainSlot(slot);
            return true;
        }
    }

    HRESULT APIENTRY D3DHooks::HookResizeBuffersA(REX::W32::IDXGISwapChain* pSwapChain, UINT BufferCount,
                                                  UINT Width, UINT Height,
                                                  REX::W32::DXGI_FORMAT NewFormat, UINT SwapChainFlags)
    {
        InFlightGuard guard{s_resizeInFlightA};
        const ResizeBuffersFunc fn = PrismaUI::Hooks::SelectHookForwarder(
            SlotStrategy(0).load(std::memory_order_acquire),
            ChainPreviousResize(0).load(std::memory_order_acquire),
            s_resizeTrampolineA.load(std::memory_order_acquire));
        if (!fn) {
            logger::critical("D3DHooks::HookResizeBuffersA entered without a trampoline");
            return E_FAIL;
        }
        if (!s_resizeOrphanedA.load(std::memory_order_acquire) &&
            !ChainOrphaned(0).load(std::memory_order_acquire))
            PrismaUI::WebRuntime::OnResizeBegin(reinterpret_cast<IDXGISwapChain*>(pSwapChain));
        const auto result = fn(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
        if (!s_resizeOrphanedA.load(std::memory_order_acquire) &&
            !ChainOrphaned(0).load(std::memory_order_acquire))
            PrismaUI::WebRuntime::OnResizeComplete(reinterpret_cast<IDXGISwapChain*>(pSwapChain), result,
                                                   static_cast<int>(Width), static_cast<int>(Height));
        return result;
    }

    HRESULT APIENTRY D3DHooks::HookResizeBuffersB(REX::W32::IDXGISwapChain* pSwapChain, UINT BufferCount,
                                                  UINT Width, UINT Height,
                                                  REX::W32::DXGI_FORMAT NewFormat, UINT SwapChainFlags)
    {
        InFlightGuard guard{s_resizeInFlightB};
        const ResizeBuffersFunc fn = PrismaUI::Hooks::SelectHookForwarder(
            SlotStrategy(1).load(std::memory_order_acquire),
            ChainPreviousResize(1).load(std::memory_order_acquire),
            s_resizeTrampolineB.load(std::memory_order_acquire));
        if (!fn) {
            logger::critical("D3DHooks::HookResizeBuffersB entered without a trampoline");
            return E_FAIL;
        }
        if (!s_resizeOrphanedB.load(std::memory_order_acquire) &&
            !ChainOrphaned(1).load(std::memory_order_acquire))
            PrismaUI::WebRuntime::OnResizeBegin(reinterpret_cast<IDXGISwapChain*>(pSwapChain));
        const auto result = fn(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
        if (!s_resizeOrphanedB.load(std::memory_order_acquire) &&
            !ChainOrphaned(1).load(std::memory_order_acquire))
            PrismaUI::WebRuntime::OnResizeComplete(reinterpret_cast<IDXGISwapChain*>(pSwapChain), result,
                                                   static_cast<int>(Width), static_cast<int>(Height));
        return result;
    }

    #include "HooksLifecycle.inl"
}
