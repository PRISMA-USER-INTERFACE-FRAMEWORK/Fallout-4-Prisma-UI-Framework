#include "DeterministicTestSupport.h"
#include "PrismaUI_F4VR_API.h"
#include "VR/SpatialPointerProtocol.h"
#include "VR/SpatialPointerRouter.h"

#include <array>
#include <cstdint>

namespace DeterministicTests
{
    namespace
    {
        namespace Pointer = PrismaUI::SpatialPointerProtocol;

        Pointer::Sample ReadyPointer(bool down, int x, int y, std::uint64_t sequence)
        {
            Pointer::Sample sample{};
            sample.active = true;
            sample.backendReady = true;
            sample.primaryDown = down;
            sample.inside = true;
            sample.hitPixelX = x;
            sample.hitPixelY = y;
            sample.sequence = sequence;
            sample.pixelWidth = 1000;
            sample.pixelHeight = 500;
            return sample;
        }

        template <std::size_t Size>
        std::array<std::uint8_t, Size> Route(
            const std::array<PrismaUI::SpatialPointerRouter::Candidate, Size>&
                candidates)
        {
            std::array<std::uint8_t, Size> winners{};
            Expect(
                PrismaUI::SpatialPointerRouter::SelectWinners(
                    candidates,
                    winners),
                "router accepts equally sized candidate and output spans");
            return winners;
        }
    }

    void TestPointerNeutralGateAndEdges()
    {
        Pointer::State state{};

        Pointer::EventBuffer heldOnEntry{};
        const auto first = Pointer::Apply(
            state,
            ReadyPointer(true, 100, 100, 1),
            heldOnEntry);
        Expect(
            first.success &&
                !state.captured &&
                heldOnEntry.count == 1,
            "held button on stream entry cannot synthesize a down edge");

        Pointer::EventBuffer neutral{};
        Expect(
            Pointer::Apply(
                state,
                ReadyPointer(false, 101, 100, 2),
                neutral)
                .success &&
                !state.requiresPrimaryRelease,
            "routed neutral sample opens the capture gate");

        Pointer::EventBuffer press{};
        Expect(
            Pointer::Apply(
                state,
                ReadyPointer(true, 102, 100, 3),
                press)
                .success &&
                state.captured,
            "fresh rising edge captures the panel");
        Expect(
            press.count == 2 &&
                press.events[0].kind == Pointer::EventKind::Move &&
                press.events[1].kind == Pointer::EventKind::Down,
            "pointer movement precedes the down edge");

        Pointer::EventBuffer release{};
        Expect(
            Pointer::Apply(
                state,
                ReadyPointer(false, 103, 100, 4),
                release)
                .success &&
                !state.captured,
            "falling edge releases capture");
        Expect(
            release.count == 2 &&
                release.events[0].buttonDown &&
                release.events[1].kind == Pointer::EventKind::Up,
            "move retains held state until the up event");
    }

    void TestPointerCancellationAndMotion()
    {
        Pointer::State state{};
        Pointer::EventBuffer neutral{};
        (void)Pointer::Apply(state, ReadyPointer(false, 200, 200, 10), neutral);
        Pointer::EventBuffer press{};
        (void)Pointer::Apply(state, ReadyPointer(true, 200, 200, 11), press);

        Pointer::EventBuffer jitter{};
        Expect(
            Pointer::Apply(state, ReadyPointer(true, 215, 205, 12), jitter)
                .success &&
                jitter.events[0].x == 200 &&
                jitter.events[0].y == 200,
            "sub-slop trigger motion stays on the press point");

        Pointer::EventBuffer drag{};
        Expect(
            Pointer::Apply(state, ReadyPointer(true, 400, 200, 13), drag)
                .success &&
                state.dragStarted &&
                drag.events[0].x == 400,
            "large intentional motion exits click slop");

        Pointer::EventBuffer cancel{};
        Expect(
            Pointer::Cancel(state, cancel, true, true) &&
                cancel.count == 2 &&
                cancel.events[0].kind == Pointer::EventKind::Move &&
                cancel.events[1].kind == Pointer::EventKind::Up,
            "capture cancellation emits forced leave then up");
        Expect(
            state.requiresPrimaryRelease,
            "cancellation restores the neutral gate");

        constexpr auto active =
            PRISMA_UI_VR_API::SpatialPointerUpdate_Active;
        Expect(
            !Pointer::MayCoalesce(
                {active, 0, 0, 0, 1},
                {active, 1, 0, 0, 1},
                active),
            "queue coalescing preserves button edges");
        Expect(
            Pointer::MayCoalesce(
                {active, 1, 0, 0, 1},
                {active, 1, 0, 0, 1},
                active),
            "equivalent movement-only samples may coalesce");
    }

    void TestPointerRouting()
    {
        using Candidate = PrismaUI::SpatialPointerRouter::Candidate;

        const std::array nearestCandidates{
            Candidate{10, 1, 0, true, true, false, true, 8.0f},
            Candidate{20, 1, 0, true, true, false, true, 2.0f},
            Candidate{30, 2, 0, true, true, false, true, 4.0f}
        };
        const auto nearest = Route(nearestCandidates);
        Expect(
            nearest[0] == 0 &&
                nearest[1] == 1 &&
                nearest[2] == 1,
            "each physical source selects its nearest panel");

        const std::array capturedCandidates{
            Candidate{10, 1, 0, true, true, true, false, 0.0f},
            Candidate{20, 1, 100, true, true, false, true, 1.0f}
        };
        const auto captured = Route(capturedCandidates);
        Expect(
            captured[0] == 1 && captured[1] == 0,
            "capture retains source ownership through off-panel drag");

        const std::array legacyCandidates{
            Candidate{10, 0, 0, false, false, false, false, 0.0f},
            Candidate{20, 0, 0, true, true, false, false, 0.0f}
        };
        const auto perView = Route(legacyCandidates);
        Expect(
            perView[0] == 1 && perView[1] == 1,
            "source zero preserves independent per-view routing");
    }
}
