#pragma once

#include <windows.h>

namespace PrismaUI::WebInput::KeySemantics {

    [[nodiscard]] constexpr bool IsSystemMessage(UINT message) noexcept {
        return message == WM_SYSKEYDOWN || message == WM_SYSKEYUP || message == WM_SYSCHAR;
    }

}
