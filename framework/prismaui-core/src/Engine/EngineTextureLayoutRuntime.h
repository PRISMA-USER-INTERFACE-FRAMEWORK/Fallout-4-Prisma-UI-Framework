#pragma once

#include "Engine/EngineTextureLayoutSupport.h"

namespace PrismaUI::Engine {

inline bool TextureLayoutBuildVerified() {
#ifdef PRISMAUI_FO4VR
    static const bool verified = [] {
        // Missing address evidence: source:docs/proof/VR-mesh-binding-layouts.md.
        logger::warn("[MeshBinding] FO4VR mesh binding is unavailable: BSGraphics::Texture is verified for 1.2.72, "
                     "but the BSShaderProperty, BSGeometry and NiAVObject layouts and the renderer-thread ordering "
                     "are not (see docs/proof/VR-mesh-binding-layouts.md)");
        return false;
    }();
    return verified;
#else
    static const bool verified = [] {
        const auto version = REX::FModule::GetExecutingModule().GetFileVersion();
        const bool ok = IsVerifiedTextureLayoutBuild(version.major(), version.minor(), version.patch());
        if (!ok) {
            logger::warn("[MeshBinding] disabled: no verified BSGraphics::Texture layout for game version {}.{}.{}",
                         version.major(), version.minor(), version.patch());
        }
        return ok;
    }();
    return verified;
#endif
}

}
