#pragma once

#include "VR/VRViewState.h"
#include "VR/WorldPanelGeometry.h"

namespace PrismaUI::SpatialPointer::HitTest
{
    [[nodiscard]] bool IntersectCapturedPlane(
        const WorldPanelGeometry::WorldPanelSurface& surface,
        const PRISMA_UI_VR_API::SpatialPointerUpdateV1& update,
        int& pixelX,
        int& pixelY) noexcept;
    [[nodiscard]] bool Hit(
        const WorldPanelGeometry::WorldPanelSurface* surface,
        const PRISMA_UI_VR_API::SpatialPointerUpdateV1& update,
        WorldPanelGeometry::WorldPanelRayHit& hit) noexcept;
}
