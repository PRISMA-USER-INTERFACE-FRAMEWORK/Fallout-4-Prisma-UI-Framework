#include "PCH.h"

#include "VR/SpatialPointer.h"

namespace PrismaUI::SpatialPointer
{
    namespace
    {
        using PRISMA_UI_VR_API::SpatialResult;
    }

    SpatialResult GetState(
        VR::PrismaViewId viewId,
        PRISMA_UI_VR_API::SpatialPointerStateV1* outState) noexcept
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
        const auto view = VR::FindView(viewId);
        if (!view) {
            return SpatialResult::InvalidView;
        }

        try {
            SpatialPointerStateV1 result{};
            result.structSize = sizeof(result);
            std::lock_guard lock(view->spatialPointerMutex);
            const auto& state = view->spatialPointer;
            result.lastApplyResult =
                static_cast<std::int32_t>(
                    state.lastApplyResult);
            result.buttonLevels =
                state.hasActive ?
                    state.active.buttonLevels :
                    0;
            result.acceptedSequence =
                state.acceptedSequence;
            result.appliedSequence =
                state.appliedSequence;
            result.hitDistance = state.hitDistance;
            result.hitUv[0] = state.hitUv[0];
            result.hitUv[1] = state.hitUv[1];
            result.pixelX = state.interaction.lastPixelX;
            result.pixelY = state.interaction.lastPixelY;
            result.replacedPendingUpdateCount =
                state.replacedPendingUpdateCount;
            result.pointerSourceId =
                state.hasActive ?
                    state.active.pointerSourceId :
                    0;
            if (state.hasActive &&
                (state.active.flags &
                 SpatialPointerUpdate_Active) != 0) {
                result.stateFlags |=
                    SpatialPointerState_Active;
            }
            if (state.sampleCount > 0) {
                result.stateFlags |=
                    SpatialPointerState_Pending;
            }
            if (state.appliedSequence > 0) {
                result.stateFlags |=
                    SpatialPointerState_Applied;
            }
            if (state.hasHit) {
                result.stateFlags |=
                    SpatialPointerState_Hit;
            }
            if (state.interaction.captured) {
                result.stateFlags |=
                    SpatialPointerState_Captured;
            }
            if (state.backendReady) {
                result.stateFlags |=
                    SpatialPointerState_BackendReady;
            }
            if (state.routed) {
                result.stateFlags |=
                    SpatialPointerState_Routed;
            }
            *outState = result;
            return SpatialResult::Ok;
        } catch (...) {
            return SpatialResult::InternalError;
        }
    }
}
