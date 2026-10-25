#include "Hooks.h"
#include "Hooks/ChainSlotOriginals.h"
#include "Hooks/InlineHookClassifier.h"
#include "PrismaUI/WebRuntime.h"
#include "Utils/ConflictChecker.h"
#include "Engine/EngineBackbuffer.h"

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

        void DiscardUnpublishedHook(PrismaUI::Hooks::TrampolineQuarantine& quarantine, void* target,
                                    const char* label);
        bool ReleaseDrainedEntry(PrismaUI::Hooks::TrampolineQuarantine& quarantine, void* target,
                                 const char* label);
        RetirementResult RemoveRetiredSlot(void* target, const unsigned char* expected, std::atomic<int>& inFlight,
                                           PrismaUI::Hooks::TrampolineQuarantine& quarantine, const char* label,
                                           int slot);
        bool RemoveRetiredPresentSlot(int slot);
        bool RemoveRetiredResizeSlot(int slot);
        void QuarantinePresentSlot(int slot, void* target, D3DHooks::PresentFunc trampoline);
        void QuarantineResizeSlot(int slot, void* target, D3DHooks::ResizeBuffersFunc trampoline);


        bool EnablePresentSlot(int slot, void* target) {
            if (PresentOrphaned(slot).load(std::memory_order_acquire)) return false;
            if (!PrismaUI::ConflictChecker::ClassifyHookTargetForInstall(target).safe) {
                logger::warn("D3DHooks: Present slot {} failed the install-time ownership policy", slot);
                return false;
            }
            auto& quarantine = PresentQuarantine();
            const auto targetKey = reinterpret_cast<uintptr_t>(target);
            auto action = PrismaUI::Hooks::DecideHookEntry(quarantine, targetKey, slot);
            if (action == PrismaUI::Hooks::HookEntryAction::Recreate) {
                const auto* entry = quarantine.At(quarantine.Find(targetKey));
                if (!entry) return false;
                if (SlotStrategy(entry->slot).load(std::memory_order_acquire) ==
                        PrismaUI::Hooks::HookInstallStrategy::DirectMinHook &&
                    PresentTrampoline(entry->slot).load(std::memory_order_acquire) ==
                        reinterpret_cast<D3DHooks::PresentFunc>(entry->trampoline)) {
                    logger::warn("D3DHooks: Present slot {} cannot re-create target {}; slot {} still publishes its trampoline",
                                 slot, target, entry->slot);
                    return false;
                }
                const int ownerSlot = entry->slot;
                auto released = reinterpret_cast<D3DHooks::PresentFunc>(entry->trampoline);
                if (!ReleaseDrainedEntry(quarantine, target, "Present")) return false;
                PresentTrampoline(ownerSlot).compare_exchange_strong(released, nullptr, std::memory_order_acq_rel);
                action = PrismaUI::Hooks::HookEntryAction::Create;
            }
            if (action == PrismaUI::Hooks::HookEntryAction::Refuse) {
                logger::warn("D3DHooks: Present slot {} cannot own target {}: {}", slot, target,
                             PrismaUI::Hooks::DescribeHookEntryRefusal(quarantine, targetKey, slot));
                return false;
            }
            D3DHooks::PresentFunc trampoline = nullptr;
            if (action == PrismaUI::Hooks::HookEntryAction::Reenable) {
                const auto* entry = quarantine.At(quarantine.Find(targetKey));
                if (!entry) return false;
                trampoline = reinterpret_cast<D3DHooks::PresentFunc>(entry->trampoline);
                logger::warn("D3DHooks: Present slot {} re-enables its quarantined entry at {}", slot, target);
            } else {
                const MH_STATUS created = MH_CreateHook(target, PresentDetour(slot),
                                                        reinterpret_cast<LPVOID*>(&trampoline));
                if (created == MH_ERROR_ALREADY_CREATED) {
                    logger::warn("D3DHooks: MinHook already owns a Present entry at {} outside the quarantine", target);
                    return false;
                }
                if (created != MH_OK || !trampoline) {
                    logger::warn("D3DHooks: MH_CreateHook(Present slot {}) failed ({})",
                                 slot, static_cast<int>(created));
                    if (created == MH_OK) DiscardUnpublishedHook(quarantine, target, "Present");
                    return false;
                }
                if (quarantine.Record(targetKey, reinterpret_cast<uintptr_t>(trampoline), slot) !=
                    PrismaUI::Hooks::QuarantineAdd::Recorded) {
                    logger::critical("D3DHooks: Present quarantine cannot record another trampoline; refusing the hook");
                    DiscardUnpublishedHook(quarantine, target, "Present");
                    return false;
                }
            }
            PresentTrampoline(slot).store(trampoline, std::memory_order_release);
            const MH_STATUS enabled = MH_EnableHook(target);
            if (enabled != MH_OK && enabled != MH_ERROR_ENABLED) {
                logger::warn("D3DHooks: MH_EnableHook(Present slot {}) failed ({})",
                             slot, static_cast<int>(enabled));
                quarantine.MarkEnabled(targetKey, false);
                if (action == PrismaUI::Hooks::HookEntryAction::Create) {
                    PresentTrampoline(slot).store(nullptr, std::memory_order_release);
                    DiscardUnpublishedHook(quarantine, target, "Present");
                }
                return false;
            }
            quarantine.MarkEnabled(targetKey, true);
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
            auto& quarantine = ResizeQuarantine();
            const auto targetKey = reinterpret_cast<uintptr_t>(target);
            auto action = PrismaUI::Hooks::DecideHookEntry(quarantine, targetKey, slot);
            if (action == PrismaUI::Hooks::HookEntryAction::Recreate) {
                const auto* entry = quarantine.At(quarantine.Find(targetKey));
                if (!entry) return false;
                if (SlotStrategy(entry->slot).load(std::memory_order_acquire) ==
                        PrismaUI::Hooks::HookInstallStrategy::DirectMinHook &&
                    ResizeTrampoline(entry->slot).load(std::memory_order_acquire) ==
                        reinterpret_cast<D3DHooks::ResizeBuffersFunc>(entry->trampoline)) {
                    logger::warn("D3DHooks: ResizeBuffers slot {} cannot re-create target {}; slot {} still publishes its trampoline",
                                 slot, target, entry->slot);
                    return false;
                }
                const int ownerSlot = entry->slot;
                auto released = reinterpret_cast<D3DHooks::ResizeBuffersFunc>(entry->trampoline);
                if (!ReleaseDrainedEntry(quarantine, target, "ResizeBuffers")) return false;
                ResizeTrampoline(ownerSlot).compare_exchange_strong(released, nullptr, std::memory_order_acq_rel);
                action = PrismaUI::Hooks::HookEntryAction::Create;
            }
            if (action == PrismaUI::Hooks::HookEntryAction::Refuse) {
                logger::warn("D3DHooks: ResizeBuffers slot {} cannot own target {}: {}", slot, target,
                             PrismaUI::Hooks::DescribeHookEntryRefusal(quarantine, targetKey, slot));
                return false;
            }
            D3DHooks::ResizeBuffersFunc trampoline = nullptr;
            if (action == PrismaUI::Hooks::HookEntryAction::Reenable) {
                const auto* entry = quarantine.At(quarantine.Find(targetKey));
                if (!entry) return false;
                trampoline = reinterpret_cast<D3DHooks::ResizeBuffersFunc>(entry->trampoline);
                logger::warn("D3DHooks: ResizeBuffers slot {} re-enables its quarantined entry at {}", slot, target);
            } else {
                const MH_STATUS created = MH_CreateHook(target, ResizeDetour(slot),
                                                        reinterpret_cast<LPVOID*>(&trampoline));
                if (created == MH_ERROR_ALREADY_CREATED) {
                    logger::warn("D3DHooks: MinHook already owns a ResizeBuffers entry at {} outside the quarantine",
                                 target);
                    return false;
                }
                if (created != MH_OK || !trampoline) {
                    logger::warn("D3DHooks: MH_CreateHook(ResizeBuffers slot {}) failed ({})",
                                 slot, static_cast<int>(created));
                    if (created == MH_OK) DiscardUnpublishedHook(quarantine, target, "ResizeBuffers");
                    return false;
                }
                if (quarantine.Record(targetKey, reinterpret_cast<uintptr_t>(trampoline), slot) !=
                    PrismaUI::Hooks::QuarantineAdd::Recorded) {
                    logger::critical("D3DHooks: ResizeBuffers quarantine cannot record another trampoline; refusing the hook");
                    DiscardUnpublishedHook(quarantine, target, "ResizeBuffers");
                    return false;
                }
            }
            ResizeTrampoline(slot).store(trampoline, std::memory_order_release);
            const MH_STATUS enabled = MH_EnableHook(target);
            if (enabled != MH_OK && enabled != MH_ERROR_ENABLED) {
                logger::warn("D3DHooks: MH_EnableHook(ResizeBuffers slot {}) failed ({})",
                             slot, static_cast<int>(enabled));
                quarantine.MarkEnabled(targetKey, false);
                if (action == PrismaUI::Hooks::HookEntryAction::Create) {
                    ResizeTrampoline(slot).store(nullptr, std::memory_order_release);
                    DiscardUnpublishedHook(quarantine, target, "ResizeBuffers");
                }
                return false;
            }
            quarantine.MarkEnabled(targetKey, true);
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
            s_chainInstallSeq[slot].fetch_add(1, std::memory_order_release);
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

namespace PrismaUI::Hooks {
    bool ChainSlotOriginalOwners(IDXGISwapChain* swapChain, void** present, void** resize) {
        return ::Hooks::D3DHooks::OriginalSwapchainSlots(swapChain, present, resize);
    }
}
