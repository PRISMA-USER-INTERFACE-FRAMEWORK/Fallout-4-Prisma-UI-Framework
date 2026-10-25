#pragma once

#include <cstdint>

namespace PrismaUI::Engine::Interface3DOffscreenSlotPolicy {

enum class Layout { OG, AE, NG, VR };

enum class SlotState { Valid, NotYetValid, UnknownLayout };

// -1 slot value at the offscreen renderer offset means "not yet assigned"; every negative value is treated as not assigned; source: ReverseEng/Fallout4_Runtime_Research@030de195 interface3d-srv-vr-q1q6
inline constexpr std::int32_t kNoSlot = -1;

[[nodiscard]] constexpr bool SlotOffsetKnown(Layout a_layout) noexcept {
    return a_layout == Layout::OG || a_layout == Layout::NG || a_layout == Layout::AE || a_layout == Layout::VR;
}

[[nodiscard]] constexpr SlotState Classify(Layout a_layout, std::int32_t a_slot) noexcept {
    if (!SlotOffsetKnown(a_layout)) return SlotState::UnknownLayout;
    return a_slot < 0 ? SlotState::NotYetValid : SlotState::Valid;
}

// RE offset: AE customRenderTarget +0x1C0, pair +0x1C4 (#905 comment 6300072506)
[[nodiscard]] constexpr Layout FlatRuntimeLayout(bool a_isOG, bool a_isNG) noexcept {
    if (a_isOG) return Layout::OG;
    if (a_isNG) return Layout::NG;
    return Layout::AE;
}

[[nodiscard]] constexpr const char* Name(SlotState a_state) noexcept {
    switch (a_state) {
        case SlotState::Valid: return "valid";
        case SlotState::NotYetValid: return "not-yet-valid";
        case SlotState::UnknownLayout: return "unknown-layout";
    }
    return "unknown-layout";
}

}
