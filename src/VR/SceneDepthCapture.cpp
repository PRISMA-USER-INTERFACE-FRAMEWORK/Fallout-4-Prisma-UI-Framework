#include "PCH.h"

#include "VR/SceneDepthCapture.h"
#include "VR/SceneDepthCaptureInternal.h"

#include "Engine/EngineVRSceneDepth.h"

#include <array>

namespace PrismaUI::SceneDepthCapture
{
    using namespace Internal;
    using namespace PrismaUI::Engine::VRSceneDepth;

    namespace
    {
        std::mutex g_lifecycleMutex;
        std::atomic<bool> g_installed = false;
        std::uintptr_t g_callsite = 0;

        template <std::size_t Size>
        [[nodiscard]] bool MatchesReadable(
            const void* address,
            const std::array<std::uint8_t, Size>& expected) noexcept
        {
            if (!address) {
                return false;
            }
            MEMORY_BASIC_INFORMATION memory{};
            if (VirtualQuery(
                    address,
                    &memory,
                    sizeof(memory)) != sizeof(memory) ||
                memory.State != MEM_COMMIT ||
                (memory.Protect &
                 (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
                return false;
            }
            const auto start =
                reinterpret_cast<std::uintptr_t>(address);
            const auto end =
                reinterpret_cast<std::uintptr_t>(
                    memory.BaseAddress) +
                memory.RegionSize;
            return start <= end &&
                   end - start >= expected.size() &&
                   std::equal(
                       expected.begin(),
                       expected.end(),
                       static_cast<const std::uint8_t*>(address));
        }

        void RestoreOriginalCall() noexcept
        {
            if (g_callsite == 0) {
                return;
            }
            try {
                REL::safe_write(
                    g_callsite,
                    kOriginalCall.data(),
                    kOriginalCall.size());
            } catch (...) {
                logger::critical(
                    "PrismaUI could not roll back a failed scene-depth hook transaction");
            }
        }
    }

    bool Install() noexcept
    {
        std::lock_guard lock(g_lifecycleMutex);
        if (g_installed.load(std::memory_order_acquire)) {
            return true;
        }
        if (!REL::Module::IsVR() ||
            REL::Module::get().version() !=
                F4SE::RUNTIME_VR_1_2_72) {
            return false;
        }

        bool patched = false;
        try {
            const auto callsite = CaptureCallsiteAddress();
            const auto target = CommitGraphicsStateAddress();
            g_callsite = callsite;
            if (!MatchesReadable(
                    reinterpret_cast<const void*>(
                        target),
                    kCommitPrologue) ||
                g_callsite < kCaptureCallsitePrefixSize ||
                !MatchesReadable(
                    reinterpret_cast<const void*>(
                        g_callsite - kCaptureCallsitePrefixSize),
                    kCallsiteBoundary)) {
                logger::error(
                    "PrismaUI scene-depth callsite failed its FO4VR 1.2.72 byte guard");
                g_callsite = 0;
                return false;
            }

            g_original = reinterpret_cast<
                CommitGraphicsState>(target);
            const auto original =
                F4SE::GetTrampoline().write_call<5>(
                    g_callsite,
                    HookCommit);
            patched = true;
            if (original != target) {
                RestoreOriginalCall();
                patched = false;
                g_callsite = 0;
                g_original = nullptr;
                return false;
            }

            g_installed.store(true, std::memory_order_release);
            logger::info(
                "PrismaUI scene-depth capture installed for FO4VR 1.2.72");
            return true;
        } catch (...) {
            if (patched && g_callsite != 0) {
                RestoreOriginalCall();
            }
            g_callsite = 0;
            g_original = nullptr;
            return false;
        }
    }

    bool IsInstalled() noexcept
    {
        return g_installed.load(std::memory_order_acquire);
    }

    void Uninstall() noexcept
    {

        Reset();
    }

    void SetCaptureRequested(bool requested) noexcept
    {
        const auto previous = g_requested.exchange(
            requested,
            std::memory_order_acq_rel);
        if (previous && !requested) {
            g_capturedEpoch.store(
                0,
                std::memory_order_release);
            std::lock_guard lock(g_captureMutex);
            g_capture = {};
        }
    }

    void Reset() noexcept
    {
        g_requested.store(false, std::memory_order_release);
        g_frameEpoch.fetch_add(1, std::memory_order_acq_rel);
        g_capturedEpoch.store(0, std::memory_order_release);
        {
            std::lock_guard lock(g_targetsMutex);
            g_submittedTargets = {};
        }
        std::lock_guard lock(g_captureMutex);
        g_capture = {};
    }
}
