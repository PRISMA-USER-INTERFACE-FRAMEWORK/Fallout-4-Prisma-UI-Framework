#pragma once

#include <cmath>

namespace PrismaUI::VirtualPointerPolicy {

inline constexpr float kDeadzone = 0.22F;
inline constexpr float kSpeedPixelsPerSecond = 1100.0F;
inline constexpr float kMaxDeltaSeconds = 0.05F;

inline float Shape(float value) noexcept
{
    const float magnitude = std::fabs(value);
    if (magnitude <= kDeadzone) return 0.0F;
    const float scaled = (magnitude - kDeadzone) / (1.0F - kDeadzone);
    const float limited = scaled > 1.0F ? 1.0F : scaled;
    return value < 0.0F ? -limited : limited;
}

inline float ClampDelta(float seconds) noexcept
{
    if (!(seconds > 0.0F)) return 0.0F;
    return seconds < kMaxDeltaSeconds ? seconds : kMaxDeltaSeconds;
}

inline int Step(int current, float axis, float seconds, int minimum, int maximum) noexcept
{
    if (maximum < minimum) return minimum;
    if (current < minimum) return minimum;
    if (current > maximum) return maximum;
    const float delta = Shape(axis) * kSpeedPixelsPerSecond * ClampDelta(seconds);
    const int offset = static_cast<int>(delta >= 0.0F ? delta + 0.5F : delta - 0.5F);
    const long long next = static_cast<long long>(current) + offset;
    if (next < static_cast<long long>(minimum)) return minimum;
    if (next > static_cast<long long>(maximum)) return maximum;
    return static_cast<int>(next);
}

inline constexpr float kStickXToScreenX = 1.0F;
inline constexpr float kStickYToScreenY = -1.0F;

inline int StepX(int current, float stickX, float seconds, int minimum, int maximum) noexcept
{
    return Step(current, kStickXToScreenX * stickX, seconds, minimum, maximum);
}

inline int StepY(int current, float stickY, float seconds, int minimum, int maximum) noexcept
{
    return Step(current, kStickYToScreenY * stickY, seconds, minimum, maximum);
}

}
