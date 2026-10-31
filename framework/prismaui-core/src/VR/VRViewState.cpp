#include "PCH.h"

#include "VR/VRViewState.h"

#include "PrismaUI/WebRuntime.h"

#include <vector>

namespace PrismaUI::VR
{
    namespace
    {
        std::atomic<bool> g_shuttingDown = false;
        std::atomic<bool> g_renderBackendOperational = false;
    }

    Registry& GetRuntime() noexcept
    {
        static Registry runtime;
        return runtime;
    }

    std::shared_ptr<VRView> FindView(PrismaViewId viewId) noexcept
    {
        if (viewId == 0) {
            return nullptr;
        }

        auto& runtime = GetRuntime();
        std::shared_lock lock(runtime.viewsMutex);
        const auto it = runtime.views.find(viewId);
        return it != runtime.views.end() ? it->second : nullptr;
    }

    std::shared_ptr<VRView> AcquireView(PrismaViewId viewId) noexcept
    {
        if (viewId == 0 || g_shuttingDown.load(std::memory_order_acquire)) {
            return nullptr;
        }

        if (auto existing = FindView(viewId)) {
            return existing;
        }

        if (!WebRuntime::IsValid(viewId)) {
            return nullptr;
        }

        auto& runtime = GetRuntime();
        std::unique_lock lock(runtime.viewsMutex);
        const auto it = runtime.views.find(viewId);
        if (it != runtime.views.end()) {
            return it->second;
        }
        if (runtime.views.size() >= kMaximumFrameworkViews) {
            logger::error(
                "PrismaUI VR refused view [{}]: the spatial registry is full ({} views)",
                viewId,
                kMaximumFrameworkViews);
            return nullptr;
        }

        auto view = std::make_shared<VRView>(viewId);
        runtime.views.emplace(viewId, view);
        return view;
    }

    void ReleaseView(PrismaViewId viewId) noexcept
    {
        std::shared_ptr<VRView> doomed;
        auto& runtime = GetRuntime();
        {
            std::unique_lock lock(runtime.viewsMutex);
            const auto it = runtime.views.find(viewId);
            if (it == runtime.views.end()) {
                return;
            }
            doomed = it->second;
            runtime.views.erase(it);
        }
        if (doomed) {
            doomed->destroying.store(true, std::memory_order_release);
        }
    }

    void PruneDeadViews() noexcept
    {
        std::vector<PrismaViewId> dead;
        auto& runtime = GetRuntime();
        {
            std::shared_lock lock(runtime.viewsMutex);
            for (const auto& [id, view] : runtime.views) {
                if (!view || !WebRuntime::IsValid(id)) {
                    dead.push_back(id);
                }
            }
        }
        for (const auto id : dead) {
            ReleaseView(id);
        }
    }

    void SyncFlags(const std::shared_ptr<VRView>& view) noexcept
    {
        if (!view) {
            return;
        }
        if (!WebRuntime::IsValid(view->id)) {
            view->destroying.store(true, std::memory_order_release);
            return;
        }
        view->hidden.store(
            WebRuntime::IsHidden(view->id),
            std::memory_order_release);
        view->focused.store(
            WebRuntime::HasFocus(view->id),
            std::memory_order_release);
    }

    bool IsShuttingDown() noexcept
    {
        return g_shuttingDown.load(std::memory_order_acquire);
    }

    void SetShuttingDown(bool shuttingDown) noexcept
    {
        g_shuttingDown.store(shuttingDown, std::memory_order_release);
    }

    bool IsRenderBackendOperational() noexcept
    {
        return g_renderBackendOperational.load(std::memory_order_acquire);
    }

    void SetRenderBackendOperational(bool operational) noexcept
    {
        g_renderBackendOperational.store(operational, std::memory_order_release);
    }
}
