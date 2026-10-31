#include "PCH.h"

#include "VR/SpatialPointer.h"

#include "VR/SpatialPointerFrame.h"
#include "VR/SpatialPointerHitTest.h"
#include "VR/SpatialPointerRouter.h"
#include "VR/SpatialPresentation.h"
#include "VR/VRPointerSink.h"

namespace PrismaUI::SpatialPointer
{
    using namespace Frame;
    using namespace HitTest;

    namespace
    {
        using PRISMA_UI_VR_API::SpatialPointerUpdateV1;
        using PRISMA_UI_VR_API::SpatialResult;
        using WorldPanelGeometry::WorldPanelRayHit;

        constexpr auto kInactiveTimeout =
            std::chrono::milliseconds(500);

        struct Prepared
        {
            FrameTarget* target = nullptr;
            std::array<
                SpatialPointerUpdateV1,
                VR::SpatialPointerRuntimeState::kSampleCapacity>
                samples{};
            std::size_t sampleCount = 0;
            SpatialPointerUpdateV1 routingSample{};
            bool hasRoutingSample = false;
            bool operational = false;
            bool timedOut = false;
            bool captured = false;
            WorldPanelRayHit routingHit{};
            bool routingHasHit = false;
        };

        [[nodiscard]] bool AppendProtocolEvents(
            VR::PrismaViewId viewId,
            const SpatialPointerProtocol::EventBuffer& events,
            std::span<VRPointerSink::SpatialPointerEvent> destination,
            std::size_t& destinationCount) noexcept
        {
            if (destinationCount > destination.size() ||
                events.count >
                destination.size() - destinationCount) {
                return false;
            }
            for (std::size_t index = 0;
                 index < events.count;
                 ++index) {
                const auto& source = events.events[index];
                auto& converted =
                    destination[destinationCount + index];
                converted.viewId = viewId;
                converted.x = source.x;
                converted.y = source.y;
                converted.deltaX = source.deltaX;
                converted.deltaY = source.deltaY;
                converted.forced = source.forced;
                converted.buttonDown = source.buttonDown;
                switch (source.kind) {
                case SpatialPointerProtocol::EventKind::Move:
                    converted.kind =
                        VRPointerSink::
                            SpatialPointerEventKind::Move;
                    break;
                case SpatialPointerProtocol::EventKind::Down:
                    converted.kind =
                        VRPointerSink::
                            SpatialPointerEventKind::Down;
                    break;
                case SpatialPointerProtocol::EventKind::Up:
                    converted.kind =
                        VRPointerSink::
                            SpatialPointerEventKind::Up;
                    break;
                case SpatialPointerProtocol::EventKind::Scroll:
                    converted.kind =
                        VRPointerSink::
                            SpatialPointerEventKind::Scroll;
                    break;
                }
            }
            destinationCount += events.count;
            return true;
        }

        void Prepare(
            FrameTarget& target,
            Prepared& prepared) noexcept
        {
            prepared = {};
            prepared.target = &target;
            target.reticle = {};
            if (!target.view) {
                return;
            }

            auto& state = target.view->spatialPointer;
            std::lock_guard lock(target.view->spatialPointerMutex);
            prepared.captured = state.interaction.captured;
            const auto now = std::chrono::steady_clock::now();
            for (std::size_t index = 0;
                 index < state.sampleCount;
                 ++index) {
                const auto source =
                    (state.sampleHead + index) %
                    state.samples.size();
                prepared.samples[prepared.sampleCount++] =
                    state.samples[source];
            }
            state.sampleHead = 0;
            state.sampleCount = 0;

            if (prepared.sampleCount > 0) {
                state.active =
                    prepared.samples[prepared.sampleCount - 1];
                state.hasActive = true;
            } else if (state.hasActive) {
                prepared.samples[0] = state.active;
                prepared.samples[0].scrollDeltaX = 0;
                prepared.samples[0].scrollDeltaY = 0;
                prepared.sampleCount = 1;
            }

            if (state.hasActive &&
                now - state.lastSubmissionTime >
                    kInactiveTimeout) {
                prepared.samples[0] = state.active;
                prepared.samples[0].flags &=
                    ~PRISMA_UI_VR_API::
                        SpatialPointerUpdate_Active;
                prepared.samples[0].buttonLevels = 0;
                prepared.samples[0].scrollDeltaX = 0;
                prepared.samples[0].scrollDeltaY = 0;
                prepared.sampleCount = 1;
                prepared.timedOut = true;
            }

            if (prepared.sampleCount == 0) {
                return;
            }
            prepared.routingSample =
                prepared.samples[prepared.sampleCount - 1];
            prepared.hasRoutingSample = true;
            prepared.operational =
                target.backendReady &&
                target.surface &&
                !target.view->hidden.load(
                    std::memory_order_acquire) &&
                !target.view->destroying.load(
                    std::memory_order_acquire);
            prepared.routingHasHit =
                prepared.operational &&
                Hit(
                    target.surface,
                    prepared.routingSample,
                    prepared.routingHit);
        }

        void ProcessPrepared(
            Prepared& prepared,
            bool winner) noexcept
        {
            auto* target = prepared.target;
            if (!target || !target->view ||
                !prepared.hasRoutingSample) {
                return;
            }

            auto& state = target->view->spatialPointer;
            std::lock_guard lock(target->view->spatialPointerMutex);
            auto interaction = state.interaction;

            if (!winner &&
                prepared.routingSample.pointerSourceId != 0) {
                SpatialPointerProtocol::EventBuffer events;
                const auto primaryDown =
                    (prepared.routingSample.buttonLevels &
                     PRISMA_UI_VR_API::
                         SpatialPointerButton_Primary) != 0;
                if (SpatialPointerProtocol::DenyCentralRoute(
                        interaction,
                        primaryDown,
                        events) &&
                    QueueProtocolEvents(
                        target->view->id,
                        events)) {
                    state.interaction = interaction;
                    state.active = prepared.routingSample;
                    state.hasActive =
                        !prepared.timedOut &&
                        (prepared.routingSample.flags &
                         PRISMA_UI_VR_API::
                             SpatialPointerUpdate_Active) != 0;
                    state.backendReady =
                        prepared.operational;
                    state.routed = false;
                    state.hasHit = false;
                    state.appliedSequence =
                        prepared.routingSample.sequence;
                    state.lastApplyResult =
                        prepared.operational ?
                            SpatialResult::Ok :
                            SpatialResult::NotReady;
                }
                return;
            }

            WorldPanelRayHit lastHit{};
            bool hasLastHit = false;
            bool allApplied = true;
            std::uint64_t appliedSequence =
                state.appliedSequence;
            auto lastApplyResult =
                state.lastApplyResult;
            std::array<
                VRPointerSink::SpatialPointerEvent,
                VR::SpatialPointerRuntimeState::kSampleCapacity *
                    SpatialPointerProtocol::EventBuffer::kCapacity>
                pendingEvents{};
            std::size_t pendingEventCount = 0;
            for (std::size_t index = 0;
                 index < prepared.sampleCount;
                 ++index) {
                const auto& update = prepared.samples[index];
                WorldPanelRayHit hit{};
                const auto hasHit =
                    prepared.operational &&
                    Hit(target->surface, update, hit);

                int capturedX = -1;
                int capturedY = -1;
                const auto hasCapturedPlane =
                    prepared.operational &&
                    target->surface &&
                    interaction.captured &&
                    IntersectCapturedPlane(
                        *target->surface,
                        update,
                        capturedX,
                        capturedY);

                SpatialPointerProtocol::Sample sample{
                    .active =
                        (update.flags &
                         PRISMA_UI_VR_API::
                             SpatialPointerUpdate_Active) != 0,
                    .backendReady = prepared.operational,
                    .primaryDown =
                        (update.buttonLevels &
                         PRISMA_UI_VR_API::
                             SpatialPointerButton_Primary) != 0,
                    .inside = hasHit,
                    .hitPixelX = hasHit ? hit.pixelX : -1,
                    .hitPixelY = hasHit ? hit.pixelY : -1,
                    .hasCapturedPlane = hasCapturedPlane,
                    .capturedPixelX = capturedX,
                    .capturedPixelY = capturedY,
                    .scrollDeltaX = update.scrollDeltaX,
                    .scrollDeltaY = update.scrollDeltaY,
                    .sequence = update.sequence,
                    .pixelWidth =
                        target->surface ?
                            target->surface->pixelWidth :
                            0,
                    .pixelHeight =
                        target->surface ?
                            target->surface->pixelHeight :
                            0
                };

                auto next = interaction;
                SpatialPointerProtocol::EventBuffer events;
                const auto result =
                    SpatialPointerProtocol::Apply(
                        next,
                        sample,
                        events);
                if (!result.success ||
                    !AppendProtocolEvents(
                        target->view->id,
                        events,
                        pendingEvents,
                        pendingEventCount)) {
                    allApplied = false;
                    break;
                }
                interaction = next;
                appliedSequence = update.sequence;
                if (hasHit) {
                    lastHit = hit;
                    hasLastHit = true;
                } else {
                    hasLastHit = false;
                }
                lastApplyResult =
                    result.disposition ==
                            SpatialPointerProtocol::
                                ApplyDisposition::Applied ?
                        SpatialResult::Ok :
                        SpatialResult::NotReady;
            }

            if (!allApplied ||
                !VRPointerSink::EnqueueSpatialPointerEvents(
                    std::span(pendingEvents).
                        first(pendingEventCount))) {
                state.lastApplyResult =
                    SpatialResult::ResourceLimit;
                return;
            }

            state.interaction = interaction;
            state.appliedSequence = appliedSequence;
            state.lastApplyResult = lastApplyResult;
            state.active = prepared.routingSample;
            state.hasActive =
                !prepared.timedOut &&
                (prepared.routingSample.flags &
                 PRISMA_UI_VR_API::
                     SpatialPointerUpdate_Active) != 0;
            state.backendReady = prepared.operational;
            state.routed =
                prepared.routingSample.pointerSourceId != 0;
            state.hasHit = hasLastHit;
            state.hitDistance =
                hasLastHit ? lastHit.distance : 0.0f;
            state.hitUv[0] = hasLastHit ? lastHit.u : 0.0f;
            state.hitUv[1] = hasLastHit ? lastHit.v : 0.0f;

            if (hasLastHit && target->surface) {
                target->reticle.visible = true;
                target->reticle.center = lastHit.position;
                target->reticle.right = target->surface->right;
                target->reticle.up = target->surface->up;
                const auto extent = (std::min)(
                    target->surface->physicalWidth,
                    target->surface->physicalHeight);
                target->reticle.physicalWidth =
                    std::clamp(extent * 0.02f, 0.5f, 4.0f);
                target->reticle.physicalHeight =
                    target->reticle.physicalWidth;
            }
        }
    }

    namespace Frame
    {
        [[nodiscard]] bool QueueProtocolEvents(
            VR::PrismaViewId viewId,
            const SpatialPointerProtocol::EventBuffer& events) noexcept
        {
            std::array<
                VRPointerSink::SpatialPointerEvent,
                SpatialPointerProtocol::EventBuffer::kCapacity>
                converted{};
            std::size_t count = 0;
            if (!AppendProtocolEvents(
                    viewId,
                    events,
                    converted,
                    count)) {
                return false;
            }
            return VRPointerSink::EnqueueSpatialPointerEvents(
                std::span(converted).first(count));
        }
    }

    void ProcessFrameBatch(
        std::span<FrameTarget> targets) noexcept
    {
        constexpr auto capacity =
            VR::kMaximumFrameworkViews;
        static_assert(
            capacity >=
            PrismaUI::SpatialPresentation::kMaximumSpatialViews);

        thread_local std::array<Prepared, capacity> prepared{};
        thread_local std::array<
            SpatialPointerRouter::Candidate,
            capacity>
            candidates{};
        thread_local std::array<std::uint8_t, capacity> winners{};

        const auto count = (std::min)(
            targets.size(),
            prepared.size());
        for (std::size_t index = 0; index < count; ++index) {
            candidates[index] = {};
            Prepare(targets[index], prepared[index]);
            const auto& item = prepared[index];
            if (!item.target || !item.target->view ||
                !item.hasRoutingSample) {
                continue;
            }
            candidates[index] = {
                .viewId = item.target->view->id,
                .sourceId =
                    item.routingSample.pointerSourceId,
                .drawOrder = item.target->drawOrder,
                .active =
                    (item.routingSample.flags &
                     PRISMA_UI_VR_API::
                         SpatialPointerUpdate_Active) != 0,
                .operational = item.operational,
                .captured = item.captured,
                .hit = item.routingHasHit,
                .hitDistance =
                    item.routingHasHit ?
                        item.routingHit.distance :
                        0.0f
            };
        }

        (void)SpatialPointerRouter::SelectWinners(
            std::span(candidates).first(count),
            std::span(winners).first(count));
        for (std::size_t index = 0; index < count; ++index) {
            const auto perView =
                candidates[index].sourceId == 0;
            ProcessPrepared(
                prepared[index],
                perView || winners[index] != 0);
        }
    }
}
