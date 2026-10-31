#pragma once

#include "VR/VRViewState.h"

#include <memory>

namespace PrismaUI::VRFocus
{

    void RequestDeferredFocus(
        const std::shared_ptr<VR::VRView>& view,
        bool pauseGame,
        bool disableFocusMenu) noexcept;

    void CancelDeferredFocus(
        const std::shared_ptr<VR::VRView>& view) noexcept;

    void ApplyDeferredFocusIfReady(
        const std::shared_ptr<VR::VRView>& view) noexcept;

    void Unfocus(VR::PrismaViewId viewId) noexcept;
}
