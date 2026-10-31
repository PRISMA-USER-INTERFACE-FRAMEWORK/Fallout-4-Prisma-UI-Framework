#pragma once

#include "VR/SpatialPointerProtocol.h"
#include "VR/VRViewState.h"

namespace PrismaUI::SpatialPointer
{
    namespace Frame
    {
        [[nodiscard]] bool QueueProtocolEvents(
            VR::PrismaViewId viewId,
            const SpatialPointerProtocol::EventBuffer& events) noexcept;
    }
}
