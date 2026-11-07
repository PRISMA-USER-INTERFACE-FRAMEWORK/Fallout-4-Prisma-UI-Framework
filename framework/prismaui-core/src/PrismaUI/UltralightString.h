#pragma once

#include <Ultralight/Ultralight.h>

#include <string>

namespace PrismaUI::WebRuntimeUltralight {

    inline std::string ToUtf8(const ultralight::String& value) {
        const auto& utf8 = value.utf8();
        return std::string(utf8.data(), utf8.size());
    }

}
