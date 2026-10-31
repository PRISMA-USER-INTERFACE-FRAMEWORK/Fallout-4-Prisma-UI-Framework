#pragma once

#include "PrismaUI_F4VR_API.h"
#include "VR/VRViewState.h"

namespace PrismaUI::VRNetworkPolicy
{

    [[nodiscard]] bool Set(
        VR::PrismaViewId viewId,
        PRISMA_UI_VR_API::NetworkAccessPolicy policy) noexcept;

    [[nodiscard]] bool Get(
        VR::PrismaViewId viewId,
        PRISMA_UI_VR_API::NetworkAccessPolicy* outPolicy) noexcept;

    void ReapplyOnDomReady(VR::PrismaViewId viewId) noexcept;

    void Forget(VR::PrismaViewId viewId) noexcept;

    [[nodiscard]] bool IsEnforceable() noexcept;
}
