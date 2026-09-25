#pragma once

#include <algorithm>
#include <cstdint>

namespace PrismaUI::PresentationHeartbeatPolicy {

struct Inputs {
    bool viewLive = false;
    bool focusSession = false;
    bool presentAlive = false;
    std::int64_t nowMs = 0;
    std::int64_t focusEpisodeStartMs = 0;
    std::int64_t lastSuccessfulPresentationMs = 0;
};

constexpr std::int64_t ReferenceMs(const Inputs& in) {
    return (std::max)(in.focusEpisodeStartMs, in.lastSuccessfulPresentationMs);
}

constexpr bool ShouldEmergencyReleaseFocus(const Inputs& in, std::int64_t staleThresholdMs) {
    if (!in.viewLive || !in.focusSession || !in.presentAlive) return false;
    const auto reference = ReferenceMs(in);
    if (reference <= 0 || in.nowMs < reference) return false;
    return in.nowMs - reference > staleThresholdMs;
}

}
