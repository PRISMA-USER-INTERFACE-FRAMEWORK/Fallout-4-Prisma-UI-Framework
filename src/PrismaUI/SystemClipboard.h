#pragma once

#include <cstddef>
#include <string>

namespace ultralight {
    class Clipboard;
}

namespace PrismaUI::SystemClipboard {
    inline constexpr std::size_t kMaxReadChars = 32768;

    std::string ReadTextUtf8(std::size_t maxChars = kMaxReadChars);
    ultralight::Clipboard& UltralightClipboard();
}
