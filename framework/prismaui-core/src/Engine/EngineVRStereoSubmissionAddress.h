#pragma once

#include <cstdint>

#include <REL/Offset.h>
#include <REL/Relocation.h>

#include "EngineVRStereoSubmission.h"

namespace PrismaUI::Engine::VRStereoSubmission {

    inline std::uintptr_t SubmitStereoTextureAddress()
    {
        return REL::Relocation<std::uintptr_t>{ REL::Offset(kSubmitStereoTextureRva) }.address();
    }

    inline std::uintptr_t FullSubmitCallsiteAddress()
    {
        return REL::Relocation<std::uintptr_t>{ REL::Offset(kFullSubmitCallsiteRva) }.address();
    }

    inline std::uintptr_t SpecialSubmitCallsiteAddress()
    {
        return REL::Relocation<std::uintptr_t>{ REL::Offset(kSpecialSubmitCallsiteRva) }.address();
    }
}
