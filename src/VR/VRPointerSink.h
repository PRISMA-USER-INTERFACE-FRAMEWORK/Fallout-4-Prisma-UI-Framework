#pragma once

#include "VR/VRViewState.h"

#include <cstdint>
#include <span>

namespace PrismaUI::VRPointerSink
{
    enum class SpatialPointerEventKind : std::uint8_t
    {
        Move,
        Down,
        Up,
        Scroll
    };

    struct SpatialPointerEvent
    {
        VR::PrismaViewId viewId = 0;
        SpatialPointerEventKind kind = SpatialPointerEventKind::Move;
        int x = -1;
        int y = -1;
        int deltaX = 0;
        int deltaY = 0;
        bool forced = false;
        bool buttonDown = false;
    };

    [[nodiscard]] bool EnqueueSpatialPointerEvents(
        std::span<const SpatialPointerEvent> events) noexcept;

    [[nodiscard]] bool ScheduleSpatialPointerEventProcessing() noexcept;

    void FlushSpatialPointerEvents() noexcept;

    void ProcessEvents() noexcept;

    void Shutdown() noexcept;
}
