#pragma once

#include "PrismaUI_F4_API.h"

namespace PRISMA_UI_API {

    using RegisterTranslationsV4Func = bool (*)(PrismaView view, const char* pluginName);

#ifdef _WIN32
    [[nodiscard]] inline bool RegisterTranslationsV4(PrismaView view, const char* pluginName) noexcept {
        const auto module = GetPrismaProviderModule();
        if (!module) return false;
        const auto fn = reinterpret_cast<RegisterTranslationsV4Func>(
            GetProcAddress(module, "PrismaUI_F4_RegisterTranslationsV4"));
        return fn ? fn(view, pluginName) : false;
    }
#endif

}
