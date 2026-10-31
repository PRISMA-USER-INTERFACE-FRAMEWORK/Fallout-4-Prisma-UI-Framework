#include "PCH.h"

#include "VR/SpatialPresentation.h"

#include "VR/SceneDepthCapture.h"
#include "VR/SpatialPresentationShared.h"
#include "VR/VRNetworkPolicy.h"

namespace PrismaUI::SpatialPresentation
{
    using namespace Shared;

    namespace
    {
        constexpr std::uint32_t apiFlavorF4Vr = 0x52563446;
        constexpr std::uint32_t spatialRevision = 1;
    }

    PRISMA_UI_VR_API::SpatialResult GetCapabilities(
        PRISMA_UI_VR_API::SpatialCapabilitiesV1* outCapabilities) noexcept
    {
        using namespace PRISMA_UI_VR_API;

        if (!outCapabilities) {
            return SpatialResult::InvalidArgument;
        }
        if (outCapabilities->structSize < sizeof(*outCapabilities)) {
            return SpatialResult::InvalidStructSize;
        }
        if (VR::IsShuttingDown()) {
            return SpatialResult::ShuttingDown;
        }

        SpatialCapabilitiesV1 capabilities{};
        capabilities.structSize = sizeof(capabilities);
        capabilities.apiFlavor = apiFlavorF4Vr;
        capabilities.spatialRevision = spatialRevision;
        capabilities.featureBits =
            SpatialFeature_FullPose |
            SpatialFeature_IndependentDimensions |
            SpatialFeature_LatestOnlyUpdates |
            SpatialFeature_AppliedSequenceQuery |
            SpatialFeature_StereoCorrectProjection |
            SpatialFeature_CompositorOverlay |
            SpatialFeature_GpuRendering |
            SpatialFeature_RefreshUpTo120Hz |
            SpatialFeature_WorldPointerInput |
            SpatialFeature_CentralPointerRouting;
        if (VRNetworkPolicy::IsEnforceable()) {
            capabilities.featureBits |= SpatialFeature_NativeNetworkPolicy;
        }
        if (SceneDepthCapture::IsInstalled()) {
            capabilities.featureBits |=
                SpatialFeature_SceneDepthOcclusion;
        }
        capabilities.coordinateSpaceMask =
            1ull << static_cast<std::uint32_t>(
                SpatialCoordinateSpace::GameWorld);
        capabilities.presentationModeMask =
            (1ull << static_cast<std::uint32_t>(
                 SpatialPresentationMode::HeadLockedQuad)) |
            (1ull << static_cast<std::uint32_t>(
                 SpatialPresentationMode::WorldBillboard)) |
            (1ull << static_cast<std::uint32_t>(
                 SpatialPresentationMode::WorldQuad));
        capabilities.supportedUpdateFlags = SupportedUpdateFlags();
        capabilities.maxPixelWidth = kMaximumPixelDimension;
        capabilities.maxPixelHeight = kMaximumPixelDimension;
        capabilities.maxSpatialViews = kMaximumSpatialViews;
        capabilities.maxAggregateSpatialPixels =
            kMaximumAggregateSpatialPixels;
        capabilities.maxRefreshRateHz = 120;
        capabilities.maxAbsoluteWorldPosition = maximumWorldMagnitude;
        capabilities.maxPhysicalDimension = maximumPhysicalDimension;
        capabilities.minQuaternionNormSquared =
            minimumQuaternionNormSquared;
        *outCapabilities = capabilities;
        return SpatialResult::Ok;
    }

    PRISMA_UI_VR_API::SpatialResult GetState(
        VR::PrismaViewId viewId,
        PRISMA_UI_VR_API::SpatialStateV1* outState) noexcept
    {
        using namespace PRISMA_UI_VR_API;

        if (!outState) {
            return SpatialResult::InvalidArgument;
        }
        if (outState->structSize < sizeof(*outState)) {
            return SpatialResult::InvalidStructSize;
        }
        if (VR::IsShuttingDown()) {
            return SpatialResult::ShuttingDown;
        }

        auto view = VR::FindView(viewId);
        if (!view) {
            return SpatialResult::InvalidView;
        }

        try {
            SpatialStateV1 state{};
            state.structSize = sizeof(state);
            const auto backendOperational =
                VR::IsRenderBackendOperational();

            std::lock_guard lock(view->spatialMutex);
            state.acceptedSequence = view->spatial.acceptedSequence;
            state.appliedSequence = view->spatial.appliedSequence;
            state.lastApplyResult = static_cast<std::int32_t>(
                backendOperational ?
                    view->spatial.lastApplyResult :
                    SpatialResult::NotReady);
            state.replacedPendingUpdateCount =
                view->spatial.replacedPendingUpdateCount;

            if (state.acceptedSequence != state.appliedSequence) {
                state.stateFlags |= SpatialState_Pending;
            }
            if (view->spatial.hasApplied) {
                const auto& applied = view->spatial.applied;
                state.coordinateSpace = applied.coordinateSpace;
                state.presentationMode = applied.presentationMode;
                state.appliedPixelWidth =
                    applied.dimensions.pixelWidth;
                state.appliedPixelHeight =
                    applied.dimensions.pixelHeight;
                state.appliedPhysicalWidth =
                    applied.dimensions.physicalWidth;
                state.appliedPhysicalHeight =
                    applied.dimensions.physicalHeight;
                state.appliedPose = applied.pose;
                state.stateFlags |= SpatialState_Applied;
                if (IsWorldMode(applied.presentationMode)) {
                    state.stateFlags |= SpatialState_Enabled;
                }
                if ((applied.flags &
                     SpatialUpdate_SceneDepthOcclusion) != 0) {
                    state.stateFlags |=
                        SpatialState_SceneDepthOcclusion;
                }
            }
            if (view->hidden.load(std::memory_order_acquire)) {
                state.stateFlags |= SpatialState_ViewHidden;
            }
            if (backendOperational && view->spatial.backendReady) {
                state.stateFlags |= SpatialState_BackendReady;
            }
            *outState = state;
            return SpatialResult::Ok;
        } catch (...) {
            return SpatialResult::InternalError;
        }
    }
}
