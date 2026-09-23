#include "PCH.h"

#include "VR/VRFocus.h"

#include "PrismaUI/WebRuntime.h"

#include <map>
#include <mutex>

namespace PrismaUI::VRFocus
{
    namespace
    {
        struct PendingFocus
        {
            bool pauseGame = false;
            bool disableFocusMenu = false;
        };

        std::mutex g_pendingMutex;
        std::map<VR::PrismaViewId, PendingFocus> g_pending;
    }

    void RequestDeferredFocus(
        const std::shared_ptr<VR::VRView>& view,
        bool pauseGame,
        bool disableFocusMenu) noexcept
    {
        if (!view) {
            return;
        }
        {
            std::lock_guard lock(g_pendingMutex);
            g_pending[view->id] = PendingFocus{pauseGame, disableFocusMenu};
        }
        view->focusRequestPending.store(true, std::memory_order_release);
        ApplyDeferredFocusIfReady(view);
    }

    void CancelDeferredFocus(
        const std::shared_ptr<VR::VRView>& view) noexcept
    {
        if (!view) {
            return;
        }
        {
            std::lock_guard lock(g_pendingMutex);
            g_pending.erase(view->id);
        }
        view->focusRequestPending.store(false, std::memory_order_release);
    }

    void ApplyDeferredFocusIfReady(
        const std::shared_ptr<VR::VRView>& view) noexcept
    {
        if (!view ||
            !view->focusRequestPending.load(std::memory_order_acquire) ||
            view->destroying.load(std::memory_order_acquire)) {
            return;
        }
        if (!WebRuntime::IsActive() || !WebRuntime::IsValid(view->id)) {
            return;
        }

        PendingFocus request;
        {
            std::lock_guard lock(g_pendingMutex);
            const auto it = g_pending.find(view->id);
            if (it == g_pending.end()) {
                view->focusRequestPending.store(false, std::memory_order_release);
                return;
            }
            request = it->second;
            g_pending.erase(it);
        }

        view->focusRequestPending.store(false, std::memory_order_release);
        if (WebRuntime::Focus(view->id, request.pauseGame, request.disableFocusMenu)) {
            view->focused.store(true, std::memory_order_release);
        }
    }

    void Unfocus(VR::PrismaViewId viewId) noexcept
    {
        {
            std::lock_guard lock(g_pendingMutex);
            g_pending.erase(viewId);
        }
        WebRuntime::Unfocus(viewId);
        if (const auto view = VR::FindView(viewId)) {
            view->focused.store(false, std::memory_order_release);
            view->focusRequestPending.store(false, std::memory_order_release);
        }
    }
}
