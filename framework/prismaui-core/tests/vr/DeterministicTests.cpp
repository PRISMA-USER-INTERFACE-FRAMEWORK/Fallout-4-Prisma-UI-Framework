#include "DeterministicTestSupport.h"
#include "PrismaUI_F4_API.h"
#include "PrismaUI_F4VR_API.h"
#include "Engine/EngineVRStereoSubmission.h"
#include "VR/GpuGeometryIndexPolicy.h"
#include "VR/GpuResourceBudget.h"
#include "VR/PresentationThreadPolicy.h"
#include "VR/RenderTargetSamplingPolicy.h"
#include "VR/VirtualFilePolicy.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

namespace DeterministicTests
{
    void TestApiLayout()
    {
        using PRISMA_UI_API::InterfaceVersion;
        Expect(
            static_cast<std::uint8_t>(InterfaceVersion::V1) == 0 &&
                static_cast<std::uint8_t>(InterfaceVersion::V4) == 3,
            "base interface ordinals remain V1=0 through V4=3");
        Expect(
            static_cast<std::uint8_t>(
                PRISMA_UI_VR_API::InterfaceVersion::V1) == 0,
            "VR extension begins its own version sequence");
        Expect(
            sizeof(PRISMA_UI_VR_API::SpatialUpdateV1) == 96 &&
                sizeof(PRISMA_UI_VR_API::SpatialStateV1) == 96,
            "spatial update/state ABI remains 96 bytes");
        Expect(
            sizeof(PRISMA_UI_VR_API::SpatialPointerUpdateV1) == 96 &&
                sizeof(PRISMA_UI_VR_API::SpatialPointerStateV1) == 96,
            "pointer update/state ABI remains 96 bytes");
        Expect(
            static_cast<std::int32_t>(
                PRISMA_UI_VR_API::SpatialResult::ResourceLimit) == -9,
            "spatial result ordinals remain stable");
        Expect(
            static_cast<std::uint64_t>(
                PRISMA_UI_API::PrismaCapability::InputRegions) == 1ull,
            "input-region capability uses the first public capability bit");
        Expect(
            sizeof(PRISMA_UI_API::InputRegion) == 16,
            "public input region ABI remains four 32-bit integers");
    }

    void TestVirtualFilePolicy()
    {
        namespace Policy = PrismaUI::VirtualFilePolicy;

        Expect(
            Policy::IsSafeRelativePath(
                "consumer/panel.html?mode=vr"),
            "ordinary relative view paths remain available");
        Expect(
            !Policy::IsSafeRelativePath(
                "consumer/%2e%2e/secret.html"),
            "encoded relative traversal is rejected");
        Expect(
            Policy::IsSafeFileUrl(
                "file:///views/consumer/panel.html",
                ""),
            "virtual-root file URLs remain available");
        Expect(
            Policy::IsSafeFileUrl(
                "file://localhost/icons/icon.png",
                "localhost"),
            "localhost virtual file URLs remain available");
        Expect(
            !Policy::IsSafeFileUrl(
                "file:///views/%2e%2e/%2e%2e/secret.txt",
                ""),
            "encoded file traversal is rejected");
        Expect(
            !Policy::IsSafeFileUrl(
                "file:///Z:/private/example.txt",
                ""),
            "host filesystem drive paths are rejected");
        Expect(
            !Policy::IsSafeFileUrl(
                "file://remotehost/views/panel.html",
                "remotehost"),
            "remote file authorities are rejected");
    }

    void TestRenderAndHookPolicies()
    {
        const auto capturedCompositorAllocation =
            PrismaUI::GpuResourceBudget::
                PlanRenderTargetAllocation(
                    12354808,
                    2734,
                    1686);
        Expect(
            capturedCompositorAllocation.valid &&
                capturedCompositorAllocation.pixels ==
                    4609524,
            "captured VR compositor render buffer fits the bounded GPU working set");

        const auto finalBudgetPixel =
            PrismaUI::GpuResourceBudget::
                PlanRenderTargetAllocation(
                    PrismaUI::GpuResourceBudget::
                            kMaximumRenderTargetPixels -
                        1,
                    1,
                    1);
        Expect(
            finalBudgetPixel.valid,
            "render-target budget accepts its final bounded pixel");
        Expect(
            !PrismaUI::GpuResourceBudget::
                 PlanRenderTargetAllocation(
                     PrismaUI::GpuResourceBudget::
                         kMaximumRenderTargetPixels,
                     1,
                     1)
                 .valid,
            "render-target budget rejects allocations beyond its hard ceiling");

        const auto clean =
            PrismaUI::RenderTargetSamplingPolicy::MakePlan(
                false,
                false,
                false);
        Expect(
            clean.valid &&
                !clean.resolveBeforeSampling &&
                !clean.suspendBoundTarget,
            "single-sample target needs no resolve");

        const auto dirty =
            PrismaUI::RenderTargetSamplingPolicy::MakePlan(
                true,
                true,
                false);
        Expect(
            dirty.valid &&
                dirty.resolveBeforeSampling &&
                dirty.suspendBoundTarget,
            "dirty bound MSAA target is suspended and resolved");

        Expect(
            !PrismaUI::RenderTargetSamplingPolicy::MakePlan(
                 false,
                 false,
                 true)
                 .valid,
            "physical read/write alias fails closed");

        constexpr std::uintptr_t callsite = 0x1000;
        constexpr std::int32_t displacement = -0x100;
        static_assert(
            PrismaUI::Engine::VRStereoSubmission::RelativeCallTarget(
                callsite,
                displacement) == 0xF05);
        Expect(
            PrismaUI::Engine::VRStereoSubmission::kFullSubmitBoundary[
                PrismaUI::Engine::VRStereoSubmission::kCallsitePrefixSize] ==
                0xE8,
            "verified full submission boundary contains the call opcode");
    }

    void TestGeometryIndexPolicy()
    {
        const std::array<std::uint32_t, 4> indices{
            0,
            1,
            99,
            2
        };
        const auto bytes = std::as_bytes(std::span(indices));
        std::vector<std::uint32_t> invalidPrefix;
        Expect(
            PrismaUI::GpuGeometryIndexPolicy::
                BuildInvalidIndexPrefix(
                    invalidPrefix,
                    bytes.data(),
                    bytes.size(),
                    3),
            "geometry index validation accepts retained buffer capacity");
        Expect(
            PrismaUI::GpuGeometryIndexPolicy::
                DrawRangeReferencesExistingVertices(
                    invalidPrefix,
                    bytes.size(),
                    0,
                    2),
            "draw accepts a valid range before unused invalid capacity");
        Expect(
            !PrismaUI::GpuGeometryIndexPolicy::
                 DrawRangeReferencesExistingVertices(
                     invalidPrefix,
                     bytes.size(),
                     2,
                     1),
            "draw rejects a consumed index beyond the vertex buffer");
        Expect(
            PrismaUI::GpuGeometryIndexPolicy::
                DrawRangeReferencesExistingVertices(
                    invalidPrefix,
                    bytes.size(),
                    3,
                    1),
            "draw accepts valid indices after unused invalid capacity");
        Expect(
            !PrismaUI::GpuGeometryIndexPolicy::DrawRangeIsValid(
                invalidPrefix,
                bytes.size(),
                4,
                1),
            "draw rejects a range beyond the staged index buffer");
    }

    void TestPresentationThreadPolicy()
    {
        PrismaUI::PresentationThreadPolicy::
            SerializedTracker<std::uint32_t>
                tracker;

        const auto first = tracker.Observe(11);
        Expect(
            first.first &&
                !first.migrated &&
                first.migrationCount == 0,
            "first serialized presentation caller establishes diagnostics");

        const auto same = tracker.Observe(11);
        Expect(
            !same.first &&
                !same.migrated &&
                same.migrationCount == 0,
            "same presentation caller does not report migration");

        const auto migrated = tracker.Observe(29);
        Expect(
            migrated.migrated &&
                migrated.migrationCount == 1,
            "serialized FO4VR presentation admits and records thread migration");

        const auto migratedAgain = tracker.Observe(11);
        Expect(
            migratedAgain.migrated &&
                migratedAgain.migrationCount == 2,
            "presentation remains operational across repeated engine-thread migration");
    }
}

int main()
{
    using namespace DeterministicTests;
    TestApiLayout();
    TestVirtualFilePolicy();
    TestWorldQuadGeometry();
    TestBillboardAndValidation();
    TestPointerNeutralGateAndEdges();
    TestPointerCancellationAndMotion();
    TestPointerRouting();
    TestRenderAndHookPolicies();
    TestGeometryIndexPolicy();
    TestPresentationThreadPolicy();
    TestInputRegionPolicy();

    if (failures != 0) {
        std::cerr << failures << " deterministic assertion(s) failed\n";
        return 1;
    }
    std::cout << "PrismaUI FO4VR deterministic tests passed\n";
    return 0;
}
