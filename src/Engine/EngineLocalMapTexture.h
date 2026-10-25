#pragma once

#include <RE/Fallout.h>

namespace PrismaUI::Engine {

    inline RE::BSScaleformExternalTexture* GetLocalMapScaleformTexture() {
        static REL::Relocation<RE::BSScaleformExternalTexture**> texture{ REL::VariantID{ 1155495, 2696851, 4804223 } };
        return *texture;
    }

    inline void GetLocalMapExtents(RE::NiPoint3& origin, RE::NiPoint3& vCorner, RE::NiPoint3& uCorner) {
        static REL::Relocation<void (*)(RE::NiPoint3&, RE::NiPoint3&, RE::NiPoint3&)> fn{ REL::ID{ 1020638, 2224090 } };
        fn(origin, vCorner, uCorner);
    }

}
