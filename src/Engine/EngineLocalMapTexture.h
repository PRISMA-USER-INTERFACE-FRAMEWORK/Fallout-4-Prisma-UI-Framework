#pragma once

#include <RE/Fallout.h>

namespace PrismaUI::Engine {

    inline RE::BSScaleformExternalTexture* GetLocalMapScaleformTexture() {
        if (!REX::FModule::IsRuntimeOG() && !REX::FModule::IsRuntimeAE()) return nullptr;
        static REL::Relocation<RE::BSScaleformExternalTexture**> g_localMapTex{ REL::ID{ 1155495, 0, 4804207 } };
        auto** pp = g_localMapTex.get();
        return pp ? *pp : nullptr;
    }

    inline void GetLocalMapExtents(RE::NiPoint3& origin, RE::NiPoint3& vCorner, RE::NiPoint3& uCorner) {
        static REL::Relocation<void (*)(RE::NiPoint3&, RE::NiPoint3&, RE::NiPoint3&)> fn{ REL::ID{ 1020638, 2224090 } };
        fn(origin, vCorner, uCorner);
    }

}
