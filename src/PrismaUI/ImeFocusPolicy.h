#pragma once

#include <cstdint>

namespace PrismaUI::ImeFocusPolicy {

enum class Association : std::uint8_t {
    kGame,
    kDefault,
    kNone
};

enum class Action : std::uint8_t {
    kNone,
    kAssociate,
    kDissociate,
    kRestore
};

constexpr Action Decide(bool prismaOwnsKeyboard, bool pageHasInputFocus, Association applied) noexcept {
    if (!prismaOwnsKeyboard) return applied == Association::kGame ? Action::kNone : Action::kRestore;
    if (pageHasInputFocus) return applied == Association::kDefault ? Action::kNone : Action::kAssociate;
    return applied == Association::kNone ? Action::kNone : Action::kDissociate;
}

constexpr bool ShowsSystemImeUi(Association applied) noexcept { return applied == Association::kDefault; }

}
