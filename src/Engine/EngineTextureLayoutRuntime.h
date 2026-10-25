#pragma once

#include "Engine/EngineTextureLayoutSupport.h"

namespace PrismaUI::Engine {

inline bool TextureLayoutBuildVerified() {
#ifdef PRISMAUI_FO4VR
    static const bool verified = [] {
        logger::warn("[MeshBinding] disabled: no verified BSGraphics::Texture layout for FO4VR");
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
