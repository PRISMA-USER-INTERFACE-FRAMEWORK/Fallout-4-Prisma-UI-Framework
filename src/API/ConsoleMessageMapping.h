#pragma once

#include "PrismaUI_F4_API.h"
#include "PrismaUI_F4_Modern_API.h"

#include <string>
#include <utility>

namespace PrismaUI::API {
    inline PRISMA_UI_API::ConsoleMessageLevel MapUltralightLogSeverity(int level) noexcept
    {
        switch (level) {
        case 0: return PRISMA_UI_API::ConsoleMessageLevel::Log;
        case 1: return PRISMA_UI_API::ConsoleMessageLevel::Warning;
        case 2: return PRISMA_UI_API::ConsoleMessageLevel::Error;
        case 3: return PRISMA_UI_API::ConsoleMessageLevel::Debug;
        case 4: return PRISMA_UI_API::ConsoleMessageLevel::Info;
        default: return PRISMA_UI_API::ConsoleMessageLevel::Log;
        }
    }

    inline PRISMA_UI_FLAT_API::ConsoleMessageLevel MapFlatLogSeverity(int level) noexcept
    {
        return static_cast<PRISMA_UI_FLAT_API::ConsoleMessageLevel>(
            static_cast<uint8_t>(MapUltralightLogSeverity(level)));
    }

    template <typename Callback, typename Mapper, typename Dispatcher>
    void DispatchConsoleMessage(PrismaView view, int levelValue, std::string message,
                                Callback callback, Mapper mapLevel, Dispatcher dispatch)
    {
        const auto level = mapLevel(levelValue);
        (void)dispatch([callback, view, level, message = std::move(message)]() {
            callback(view, level, message.c_str());
        }, view);
    }
}
