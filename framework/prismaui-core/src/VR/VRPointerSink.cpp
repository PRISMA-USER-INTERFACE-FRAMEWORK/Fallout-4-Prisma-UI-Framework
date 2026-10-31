#include "PCH.h"

#include "VR/VRPointerSink.h"

#include "PrismaUI/WebRuntime.h"

#include <deque>
#include <mutex>

namespace PrismaUI::VRPointerSink
{
    namespace
    {
        constexpr std::size_t kMaximumQueuedEvents = 512;
        constexpr int kLeftButton = 0;

        std::mutex g_queueMutex;
        std::deque<SpatialPointerEvent> g_queue;

        std::mutex g_dispatchMutex;
        VR::PrismaViewId g_dispatchTarget = 0;

        void RetargetIfNeeded(VR::PrismaViewId viewId) noexcept
        {
            if (g_dispatchTarget == viewId) {
                return;
            }
            WebRuntime::SetInputTargetView(viewId);
            g_dispatchTarget = viewId;
        }

        void Dispatch(const SpatialPointerEvent& event) noexcept
        {
            if (event.viewId == 0 || !WebRuntime::IsValid(event.viewId)) {
                return;
            }
            RetargetIfNeeded(event.viewId);

            switch (event.kind) {
            case SpatialPointerEventKind::Move:
                WebRuntime::SendMouseMove(
                    event.x,
                    event.y,
                    event.buttonDown ? 1u : 0u,
                    event.forced,
                    false);
                break;
            case SpatialPointerEventKind::Down:
                WebRuntime::SendMouseClick(
                    event.x,
                    event.y,
                    kLeftButton,
                    false,
                    1,
                    1u,
                    false);
                break;
            case SpatialPointerEventKind::Up:
                WebRuntime::SendMouseClick(
                    event.x,
                    event.y,
                    kLeftButton,
                    true,
                    1,
                    0u,
                    false);
                break;
            case SpatialPointerEventKind::Scroll:
                WebRuntime::SendMouseWheel(
                    event.x,
                    event.y,
                    event.deltaX,
                    event.deltaY,
                    event.buttonDown ? 1u : 0u,
                    false);
                break;
            }
        }
    }

    bool EnqueueSpatialPointerEvents(
        std::span<const SpatialPointerEvent> events) noexcept
    {
        if (events.empty()) {
            return true;
        }
        try {
            std::lock_guard lock(g_queueMutex);
            if (g_queue.size() + events.size() > kMaximumQueuedEvents) {
                return false;
            }
            g_queue.insert(g_queue.end(), events.begin(), events.end());
        } catch (...) {
            return false;
        }
        return true;
    }

    bool ScheduleSpatialPointerEventProcessing() noexcept
    {
        ProcessEvents();
        return true;
    }

    void FlushSpatialPointerEvents() noexcept
    {
        try {
            std::lock_guard lock(g_queueMutex);
            g_queue.clear();
        } catch (...) {
        }
    }

    void ProcessEvents() noexcept
    {
        std::deque<SpatialPointerEvent> batch;
        try {
            std::lock_guard lock(g_queueMutex);
            batch.swap(g_queue);
        } catch (...) {
            return;
        }
        if (batch.empty()) {
            return;
        }

        try {
            std::lock_guard lock(g_dispatchMutex);
            for (const auto& event : batch) {
                Dispatch(event);
            }
        } catch (...) {
        }
    }

    void Shutdown() noexcept
    {
        FlushSpatialPointerEvents();
        try {
            std::lock_guard lock(g_dispatchMutex);
            if (g_dispatchTarget != 0) {
                WebRuntime::SetInputTargetView(0);
                g_dispatchTarget = 0;
            }
        } catch (...) {
        }
    }
}
