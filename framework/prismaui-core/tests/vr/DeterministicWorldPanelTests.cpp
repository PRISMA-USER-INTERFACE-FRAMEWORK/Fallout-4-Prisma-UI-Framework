#include "DeterministicTestSupport.h"
#include "PrismaUI_F4VR_API.h"
#include "VR/WorldPanelGeometry.h"

namespace DeterministicTests
{
    namespace
    {
        PRISMA_UI_VR_API::SpatialUpdateV1 MakeWorldQuad()
        {
            PRISMA_UI_VR_API::SpatialUpdateV1 update{};
            update.structSize = sizeof(update);
            update.coordinateSpace =
                PRISMA_UI_VR_API::SpatialCoordinateSpace::GameWorld;
            update.presentationMode =
                PRISMA_UI_VR_API::SpatialPresentationMode::WorldQuad;
            update.sequence = 1;
            update.pose.position[0] = 10.0f;
            update.pose.position[1] = 20.0f;
            update.pose.position[2] = 30.0f;
            update.pose.orientation[3] = 1.0f;
            update.dimensions.pixelWidth = 1000;
            update.dimensions.pixelHeight = 500;
            update.dimensions.physicalWidth = 200.0f;
            update.dimensions.physicalHeight = 100.0f;
            return update;
        }
    }

    void TestWorldQuadGeometry()
    {
        namespace Geometry = PrismaUI::WorldPanelGeometry;

        const auto update = MakeWorldQuad();
        Geometry::WorldPanelPlacement placement{};
        Expect(
            Geometry::MakePlacement(update, placement),
            "valid oriented world update creates a placement");

        Geometry::WorldPanelSurface surface{};
        Expect(
            Geometry::ResolveSurface(placement, {}, surface),
            "identity quaternion resolves an oriented surface");
        Expect(
            Near(surface.right.x, 1.0f) &&
                Near(surface.up.y, 1.0f) &&
                Near(surface.normal.z, 1.0f),
            "identity quaternion preserves local panel basis");
        Expect(
            Near(surface.corners[0].x, -90.0f) &&
                Near(surface.corners[0].y, 70.0f),
            "top-left corner uses independent physical dimensions");

        Geometry::WorldPanelRayHit centerHit{};
        Expect(
            Geometry::IntersectRay(
                surface,
                {10.0f, 20.0f, 130.0f},
                {0.0f, 0.0f, -2.0f},
                200.0f,
                centerHit),
            "two-sided ray hits the world quad center");
        Expect(
            Near(centerHit.u, 0.5f) &&
                Near(centerHit.v, 0.5f) &&
                centerHit.pixelX == 500 &&
                centerHit.pixelY == 250,
            "center hit maps to the center raster pixel");

        Geometry::WorldPanelRayHit miss{};
        Expect(
            !Geometry::IntersectRay(
                surface,
                {500.0f, 500.0f, 130.0f},
                {0.0f, 0.0f, -1.0f},
                200.0f,
                miss),
            "ray outside physical bounds fails closed");
    }

    void TestBillboardAndValidation()
    {
        namespace Geometry = PrismaUI::WorldPanelGeometry;
        auto update = MakeWorldQuad();
        update.presentationMode =
            PRISMA_UI_VR_API::SpatialPresentationMode::WorldBillboard;

        Geometry::WorldPanelPlacement placement{};
        Expect(
            Geometry::MakePlacement(update, placement),
            "billboard update ignores caller orientation");

        Geometry::WorldPanelSurface surface{};
        Expect(
            Geometry::ResolveSurface(
                placement,
                {10.0f, 20.0f, 130.0f},
                surface),
            "billboard faces the supplied eye midpoint");
        Expect(
            Near(surface.normal.z, 1.0f),
            "billboard normal points toward the eye midpoint");

        update.reserved[2] = 1;
        Expect(
            !Geometry::MakePlacement(update, placement),
            "non-zero reserved fields fail closed");

        update = MakeWorldQuad();
        update.pose.orientation[0] = 0.0f;
        update.pose.orientation[1] = 0.0f;
        update.pose.orientation[2] = 0.0f;
        update.pose.orientation[3] = 0.0f;
        Expect(
            !Geometry::MakePlacement(update, placement),
            "zero quaternion is rejected");
    }
}
