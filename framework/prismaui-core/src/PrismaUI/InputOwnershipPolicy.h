#pragma once

#include <cstdint>

namespace PrismaUI::InputOwnershipPolicy {

enum class CaptureOwner : std::uint8_t {
    kNone,
    kPassiveDock,
    kFocusedView
};

enum class CursorVisibilityAction : std::uint8_t {
    kNone,
    kHideVanilla,
    kRestoreVanilla
};

constexpr CaptureOwner AfterFocusedViewRelease(bool dockVisible) noexcept {
    return dockVisible ? CaptureOwner::kPassiveDock : CaptureOwner::kNone;
}

constexpr CaptureOwner OwnerFromFocusedInput(bool focusedViewOwnsInput) noexcept {
    return focusedViewOwnsInput ? CaptureOwner::kFocusedView : CaptureOwner::kPassiveDock;
}

constexpr bool ShouldRouteMouse(CaptureOwner owner, bool focusedViewAcceptsMouse) noexcept {
    return owner == CaptureOwner::kPassiveDock ||
           (owner == CaptureOwner::kFocusedView && focusedViewAcceptsMouse);
}

constexpr bool ShouldRouteFocusedClick(CaptureOwner owner, bool focusedViewAcceptsMouse) noexcept {
    return owner == CaptureOwner::kFocusedView && focusedViewAcceptsMouse;
}

constexpr bool PrismaOwnsCursor(CaptureOwner owner, bool dockPointerInteractive = false) noexcept {
    return owner == CaptureOwner::kFocusedView ||
           (owner == CaptureOwner::kPassiveDock && dockPointerInteractive);
}

constexpr CursorVisibilityAction NextCursorVisibilityAction(
    CaptureOwner owner, bool vanillaCursorHiddenByPrisma) noexcept {
    if (PrismaOwnsCursor(owner)) {
        return vanillaCursorHiddenByPrisma ? CursorVisibilityAction::kNone
                                           : CursorVisibilityAction::kHideVanilla;
    }
    return vanillaCursorHiddenByPrisma ? CursorVisibilityAction::kRestoreVanilla
                                       : CursorVisibilityAction::kNone;
}

}
