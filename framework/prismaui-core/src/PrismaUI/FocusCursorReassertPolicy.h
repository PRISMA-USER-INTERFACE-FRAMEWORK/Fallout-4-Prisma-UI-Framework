#pragma once

#include <cstdint>
#include <string_view>

namespace PrismaUI::FocusCursorReassertPolicy {

enum class Action : std::uint8_t {
    None = 0,
    ClearConstraints = 1,
};

[[nodiscard]] constexpr bool IsCursorConstrainingMenu(std::string_view menu) noexcept
{
    return menu == "PauseMenu";
}

[[nodiscard]] constexpr Action Decide(std::string_view menu, bool prismaOwnsCursor, bool menuHidden) noexcept
{
    if (!prismaOwnsCursor || !menuHidden || !IsCursorConstrainingMenu(menu)) return Action::None;
    return Action::ClearConstraints;
}

}
