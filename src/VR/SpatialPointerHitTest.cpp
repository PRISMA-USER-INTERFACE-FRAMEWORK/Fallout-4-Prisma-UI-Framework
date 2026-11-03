#include "PCH.h"

#include "VR/SpatialPointerHitTest.h"

namespace PrismaUI::SpatialPointer
{
    namespace HitTest
    {
        using PRISMA_UI_VR_API::SpatialPointerUpdateV1;
        using WorldPanelGeometry::Vec3;
        using WorldPanelGeometry::WorldPanelRayHit;
        using WorldPanelGeometry::WorldPanelSurface;

        [[nodiscard]] bool IntersectCapturedPlane(
            const WorldPanelSurface& surface,
            const SpatialPointerUpdateV1& update,
            int& pixelX,
            int& pixelY) noexcept
        {
            const Vec3 origin{
                update.rayOrigin[0],
                update.rayOrigin[1],
                update.rayOrigin[2]
            };
            const Vec3 direction{
                update.rayDirection[0],
                update.rayDirection[1],
                update.rayDirection[2]
            };
            const auto dot = [](const Vec3& left, const Vec3& right) {
                return left.x * right.x +
                       left.y * right.y +
                       left.z * right.z;
            };
            const auto subtract = [](const Vec3& left, const Vec3& right) {
                return Vec3{
                    left.x - right.x,
                    left.y - right.y,
                    left.z - right.z
                };
            };
            const auto denominator =
                dot(direction, surface.normal);
            if (!std::isfinite(denominator) ||
                std::fabs(denominator) <= 1.0e-6f) {
                return false;
            }
            const auto distance =
                dot(
                    subtract(surface.center, origin),
                    surface.normal) /
                denominator;
            if (!std::isfinite(distance) ||
                distance < 0.0f ||
                distance > update.maxDistance) {
                return false;
            }
            const Vec3 position{
                origin.x + direction.x * distance,
                origin.y + direction.y * distance,
                origin.z + direction.z * distance
            };
            const auto relative =
                subtract(position, surface.center);
            const auto u =
                dot(relative, surface.right) /
                    surface.physicalWidth +
                0.5f;
            const auto v =
                0.5f -
                dot(relative, surface.up) /
                    surface.physicalHeight;
            if (!std::isfinite(u) || !std::isfinite(v)) {
                return false;
            }
            pixelX = static_cast<int>(std::lround(
                u * static_cast<float>(surface.pixelWidth)));
            pixelY = static_cast<int>(std::lround(
                v * static_cast<float>(surface.pixelHeight)));
            return true;
        }

        [[nodiscard]] bool Hit(
            const WorldPanelSurface* surface,
            const SpatialPointerUpdateV1& update,
            WorldPanelRayHit& hit) noexcept
        {
            if (!surface ||
                (update.flags &
                 PRISMA_UI_VR_API::
                     SpatialPointerUpdate_Active) == 0) {
                return false;
            }
            return WorldPanelGeometry::IntersectRay(
                *surface,
                {
                    update.rayOrigin[0],
                    update.rayOrigin[1],
                    update.rayOrigin[2]
                },
                {
                    update.rayDirection[0],
                    update.rayDirection[1],
                    update.rayDirection[2]
                },
                update.maxDistance,
                hit);
        }
    }
}
