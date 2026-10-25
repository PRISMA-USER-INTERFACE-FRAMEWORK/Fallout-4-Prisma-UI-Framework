#include "PCH.h"

#include "VR/SpatialPointer.h"

#include "VR/SpatialPointerFrame.h"
#include "VR/VRPointerSink.h"

namespace PrismaUI::SpatialPointer
{
    using namespace Frame;

    namespace
    {
        using PRISMA_UI_VR_API::SpatialPointerUpdateV1;
        using PRISMA_UI_VR_API::SpatialResult;

        constexpr float kMaximumRayMagnitude = 1.0e8f;
        constexpr float kMinimumDirectionNormSquared = 1.0e-8f;

        [[nodiscard]] bool IsFiniteBounded(
            float value) noexcept
        {
            return std::isfinite(value) &&
                   std::fabs(value) <= kMaximumRayMagnitude;
        }

        [[nodiscard]] bool ValidateAndNormalize(
            SpatialPointerUpdateV1& update) noexcept
        {
            using namespace PRISMA_UI_VR_API;

            if (update.coordinateSpace !=
                    SpatialCoordinateSpace::GameWorld ||
                (update.flags & ~SpatialPointerUpdate_Active) != 0 ||
                (update.buttonLevels &
                 ~SpatialPointerButton_Primary) != 0 ||
                update.sequence == 0 ||
                std::any_of(
                    std::begin(update.reserved),
                    std::end(update.reserved),
                    [](std::uint32_t value) {
                        return value != 0;
                    })) {
                return false;
            }

            if ((update.flags &
                 SpatialPointerUpdate_Active) == 0) {
                update.buttonLevels = 0;
                update.scrollDeltaX = 0;
                update.scrollDeltaY = 0;
                update.rayOrigin[0] = 0.0f;
                update.rayOrigin[1] = 0.0f;
                update.rayOrigin[2] = 0.0f;
                update.rayDirection[0] = 0.0f;
                update.rayDirection[1] = 0.0f;
                update.rayDirection[2] = 1.0f;
                update.maxDistance = 0.0f;
                return true;
            }

            if (!IsFiniteBounded(update.maxDistance) ||
                update.maxDistance <= 0.0f) {
                return false;
            }

            float directionNormSquared = 0.0f;
            for (std::size_t index = 0; index < 3; ++index) {
                if (!IsFiniteBounded(update.rayOrigin[index]) ||
                    !IsFiniteBounded(
                        update.rayDirection[index])) {
                    return false;
                }
                directionNormSquared +=
                    update.rayDirection[index] *
                    update.rayDirection[index];
            }
            if (!std::isfinite(directionNormSquared) ||
                directionNormSquared <
                    kMinimumDirectionNormSquared) {
                return false;
            }
            const auto inverseNorm =
                1.0f / std::sqrt(directionNormSquared);
            for (auto& component : update.rayDirection) {
                component *= inverseNorm;
            }
            return true;
        }

        [[nodiscard]] SpatialPointerProtocol::QueueSampleKey
            QueueKey(
                const SpatialPointerUpdateV1& update) noexcept
        {
            return {
                .flags = update.flags,
                .buttonLevels = update.buttonLevels,
                .scrollDeltaX = update.scrollDeltaX,
                .scrollDeltaY = update.scrollDeltaY,
                .pointerSourceId = update.pointerSourceId
            };
        }

        [[nodiscard]] bool ApplyCancel(
            const std::shared_ptr<VR::VRView>& view,
            bool forced,
            bool requireNeutral) noexcept
        {
            if (!view) {
                return false;
            }
            std::lock_guard lock(view->spatialPointerMutex);
            auto next = view->spatialPointer.interaction;
            SpatialPointerProtocol::EventBuffer events;
            if (!SpatialPointerProtocol::Cancel(
                    next,
                    events,
                    forced,
                    requireNeutral) ||
                !QueueProtocolEvents(view->id, events)) {
                return false;
            }

            view->spatialPointer.interaction = next;
            view->spatialPointer.sampleHead = 0;
            view->spatialPointer.sampleCount = 0;
            view->spatialPointer.hasActive = false;
            view->spatialPointer.backendReady = false;
            view->spatialPointer.routed = false;
            view->spatialPointer.hasHit = false;
            view->spatialPointer.hitDistance = 0.0f;
            view->spatialPointer.hitUv[0] = 0.0f;
            view->spatialPointer.hitUv[1] = 0.0f;
            view->spatialPointer.lastApplyResult =
                SpatialResult::Ok;
            return true;
        }
    }

    SpatialResult SubmitUpdate(
        VR::PrismaViewId viewId,
        const SpatialPointerUpdateV1* update) noexcept
    {
        if (!update) {
            return SpatialResult::InvalidArgument;
        }
        if (update->structSize < sizeof(*update)) {
            return SpatialResult::InvalidStructSize;
        }
        if (VR::IsShuttingDown()) {
            return SpatialResult::ShuttingDown;
        }

        const auto view = VR::FindView(viewId);
        if (!view) {
            return SpatialResult::InvalidView;
        }
        SpatialPointerUpdateV1 normalized = *update;
        if (!ValidateAndNormalize(normalized)) {
            return SpatialResult::InvalidArgument;
        }

        try {
            std::lock_guard lock(view->spatialPointerMutex);
            auto& state = view->spatialPointer;
            if (normalized.sequence <= state.acceptedSequence) {
                return SpatialResult::StaleSequence;
            }

            auto replaced = false;
            if (state.sampleCount == state.samples.size()) {
                const auto last =
                    (state.sampleHead + state.sampleCount - 1) %
                    state.samples.size();
                if (!SpatialPointerProtocol::MayCoalesce(
                        QueueKey(state.samples[last]),
                        QueueKey(normalized),
                        PRISMA_UI_VR_API::
                            SpatialPointerUpdate_Active)) {
                    return SpatialResult::ResourceLimit;
                }
                state.samples[last] = normalized;
                ++state.replacedPendingUpdateCount;
                replaced = true;
            } else {
                const auto tail =
                    (state.sampleHead + state.sampleCount) %
                    state.samples.size();
                state.samples[tail] = normalized;
                ++state.sampleCount;
            }

            state.acceptedSequence = normalized.sequence;
            state.lastSubmissionTime =
                std::chrono::steady_clock::now();
            state.lastApplyResult = SpatialResult::NotReady;
            return replaced ?
                SpatialResult::PendingUpdateReplaced :
                SpatialResult::Ok;
        } catch (...) {
            return SpatialResult::InternalError;
        }
    }

    SpatialResult Cancel(VR::PrismaViewId viewId) noexcept
    {
        const auto view = VR::FindView(viewId);
        if (!view) {
            return SpatialResult::InvalidView;
        }
        return CancelForView(view, false) ?
            SpatialResult::Ok :
            SpatialResult::ResourceLimit;
    }

    bool CancelForView(
        const std::shared_ptr<VR::VRView>& view,
        bool dispatchEvents) noexcept
    {
        if (!view) {
            return false;
        }
        if (ApplyCancel(view, true, true)) {
            if (!dispatchEvents) {
                return true;
            }
            return VRPointerSink::
                ScheduleSpatialPointerEventProcessing();
        }
        if (!dispatchEvents) {
            return false;
        }

        VRPointerSink::ProcessEvents();
        if (!ApplyCancel(view, true, true)) {
            logger::error(
                "View [{}] could not enqueue its forced pointer release",
                view->id);
            return false;
        }
        VRPointerSink::ProcessEvents();
        return true;
    }

    void HandleBackendUnavailable() noexcept
    {
        std::vector<std::shared_ptr<VR::VRView>> snapshot;
        auto& runtime = VR::GetRuntime();
        {
            std::shared_lock lock(runtime.viewsMutex);
            snapshot.reserve(runtime.views.size());
            for (const auto& [id, view] : runtime.views) {
                (void)id;
                if (view) {
                    snapshot.push_back(view);
                }
            }
        }
        for (const auto& view : snapshot) {
            (void)ApplyCancel(view, true, true);
            std::lock_guard lock(view->spatialPointerMutex);
            view->spatialPointer.lastApplyResult =
                SpatialResult::NotReady;
        }
    }

    void Shutdown() noexcept
    {
        HandleBackendUnavailable();
        VRPointerSink::FlushSpatialPointerEvents();
    }
}
