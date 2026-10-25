#pragma once

namespace Hooks::EngineStereoSubmissionPolicy
{
    struct Bounds
    {
        float uMin;
        float vMin;
        float uMax;
        float vMax;
    };

    inline constexpr Bounds kLeftBounds{0.0f, 0.0f, 0.5f, 1.0f};
    inline constexpr Bounds kRightBounds{0.5f, 0.0f, 1.0f, 1.0f};
}
