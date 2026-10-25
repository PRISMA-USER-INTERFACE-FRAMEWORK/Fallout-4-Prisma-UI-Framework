#pragma once

namespace PrismaUI::SceneDepthDrawPolicy {

enum class Mode {
    kColorOnly,
    kOccluded,
};

[[nodiscard]] constexpr Mode Decide(bool requested, bool snapshotValid) noexcept
{
    if (requested && snapshotValid) {
        return Mode::kOccluded;
    }
    return Mode::kColorOnly;
}

[[nodiscard]] constexpr bool ReportOcclusionApplied(bool requested, bool snapshotValid) noexcept
{
    return requested && snapshotValid;
}

}
