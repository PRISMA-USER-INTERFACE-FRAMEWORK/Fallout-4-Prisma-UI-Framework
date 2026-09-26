#pragma once

#include <RE/Fallout.h>

namespace PrismaUI::Engine {

    using GetTextureFn = void (*)(const char* apFilename, bool abCheckHardDrive,
                                  RE::NiPointer<RE::NiTexture>& aspTexture, bool abCubeMap,
                                  bool abSRGB, bool abAllowDegrade);

    inline REL::Relocation<GetTextureFn>& EngineGetTextureFn() {
        static REL::Relocation<GetTextureFn> fn{ REL::ID{ 1375091, 2316549 } };
        return fn;
    }

    inline void CallEngineGetTexture(const char* path, RE::NiPointer<RE::NiTexture>& out) {
        __try {
            EngineGetTextureFn()(path, true, out, false, true, true);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

}
