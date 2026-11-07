#include "PrismaUI/InputOwnershipPolicy.h"
#include "PrismaUI/VirtualPointerClickState.h"

#include <cassert>
#include <cstdint>

using namespace PrismaUI::InputOwnershipPolicy;
using namespace PrismaUI::VirtualPointerClick;

static constexpr Owner kViewA{ 9, 1 };
static constexpr Owner kViewB{ 10, 1 };
static constexpr Owner kViewAReloaded{ 9, 2 };
static constexpr Owner kNoView{ 0, 0 };

static void TestFocusedClickRouting()
{
    assert(ShouldRouteFocusedClick(CaptureOwner::kFocusedView, true));
    assert(!ShouldRouteFocusedClick(CaptureOwner::kFocusedView, false));
    assert(!ShouldRouteFocusedClick(CaptureOwner::kPassiveDock, true));
    assert(!ShouldRouteFocusedClick(CaptureOwner::kPassiveDock, false));
    assert(!ShouldRouteFocusedClick(CaptureOwner::kNone, true));
}

static void TestPhysicalMouseRoutingUnchanged()
{
    assert(ShouldRouteMouse(CaptureOwner::kPassiveDock, false));
    assert(ShouldRouteMouse(CaptureOwner::kPassiveDock, true));
    assert(ShouldRouteMouse(CaptureOwner::kFocusedView, true));
    assert(!ShouldRouteMouse(CaptureOwner::kFocusedView, false));
    assert(!ShouldRouteMouse(CaptureOwner::kNone, true));
}

static void TestOwnerDerivation()
{
    assert(OwnerFromFocusedInput(true) == CaptureOwner::kFocusedView);
    assert(OwnerFromFocusedInput(false) == CaptureOwner::kPassiveDock);
}

static void TestDownUpSameOwner()
{
    State s;
    assert(BeginClick(s, true, kViewA) == Begin::kAdmitted);
    assert(s.held);
    assert(s.owner == kViewA);
    assert(EndClick(s, kViewA) == End::kReleaseOwner);
    assert(!s.held);
    assert(s.owner == kNoView);
}

static void TestSecondBeginWhileHeldIsRejected()
{
    State s;
    assert(BeginClick(s, true, kViewA) == Begin::kAdmitted);
    assert(BeginClick(s, true, kViewA) == Begin::kRejected);
    assert(BeginClick(s, true, kViewB) == Begin::kRejected);
}

static void TestRegionRejectionDoesNotBegin()
{
    State s;
    assert(BeginClick(s, false, kViewA) == Begin::kRejected);
    assert(!s.held);
}

static void TestInvalidOwnerDoesNotBegin()
{
    State s;
    assert(BeginClick(s, true, kNoView) == Begin::kRejected);
    assert(BeginClick(s, true, Owner{ 9, 0 }) == Begin::kRejected);
    assert(!s.held);
}

static void TestFocusLossWhileHeld()
{
    State s;
    assert(BeginClick(s, true, kViewA) == Begin::kAdmitted);
    assert(!CancelClick(s, kNoView));
    assert(!s.held);
    assert(BeginClick(s, true, kViewA) == Begin::kAdmitted);
}

static void TestHandoffToAnotherViewWhileHeld()
{
    State s;
    assert(BeginClick(s, true, kViewA) == Begin::kAdmitted);
    assert(!CancelClick(s, kViewB));
    assert(!s.held);
    assert(BeginClick(s, true, kViewB) == Begin::kAdmitted);
    assert(s.owner == kViewB);
}

static void TestReloadGenerationChangeWhileHeld()
{
    State s;
    assert(BeginClick(s, true, kViewA) == Begin::kAdmitted);
    assert(!CancelClick(s, kViewAReloaded));
    assert(!s.held);
}

static void TestDestroyWhileHeld()
{
    State s;
    assert(BeginClick(s, true, kViewA) == Begin::kAdmitted);
    assert(!CancelClick(s, kNoView));
    assert(!s.held);
    assert(s.owner == kNoView);
}

static void TestSameOwnerCancelReleases()
{
    State s;
    assert(BeginClick(s, true, kViewA) == Begin::kAdmitted);
    assert(CancelClick(s, kViewA));
    assert(!s.held);
}

static void TestCancelWithoutHoldIsInert()
{
    State s;
    assert(!CancelClick(s, kViewA));
    assert(!s.held);
}

static void TestReleaseIntoAnotherTargetIsDiscarded()
{
    State s;
    assert(BeginClick(s, true, kViewA) == Begin::kAdmitted);
    assert(EndClick(s, kViewB) == End::kDiscarded);
    assert(!s.held);
}

static void TestReleaseOutsideRegionStillReachesOwner()
{
    State s;
    assert(BeginClick(s, true, kViewA) == Begin::kAdmitted);
    assert(EndClick(s, kViewA) == End::kReleaseOwner);
    assert(!s.held);
}

static void TestCancelOutsideRegionStillReachesOwner()
{
    State s;
    assert(BeginClick(s, true, kViewA) == Begin::kAdmitted);
    assert(CancelClick(s, kViewA));
    assert(!s.held);
    assert(s.owner == kNoView);
}

static void TestReleaseAfterOwnerInvalidDiscards()
{
    State s;
    assert(BeginClick(s, true, kViewA) == Begin::kAdmitted);
    assert(EndClick(s, kNoView) == End::kDiscarded);
    assert(EndClick(s, Owner{ 9, 0 }) == End::kNothing);
    assert(!s.held);
}

static void TestReloadGenerationReleaseDiscards()
{
    State s;
    assert(BeginClick(s, true, kViewA) == Begin::kAdmitted);
    assert(EndClick(s, kViewAReloaded) == End::kDiscarded);
    assert(!s.held);
}

static void TestDiscardOwnerViewDropsOnlyItsOwnHold()
{
    State s;
    assert(BeginClick(s, true, kViewA) == Begin::kAdmitted);
    assert(DiscardIfOwnerView(s, kViewA.view));
    assert(!s.held);
    assert(s.owner == kNoView);
}

static void TestDiscardUnrelatedViewKeepsOwner()
{
    State s;
    assert(BeginClick(s, true, kViewA) == Begin::kAdmitted);
    assert(!DiscardIfOwnerView(s, kViewB.view));
    assert(s.held);
    assert(s.owner == kViewA);
    assert(EndClick(s, kViewA) == End::kReleaseOwner);
}

static void TestDiscardFailedDomCleanupOfUnrelatedViewKeepsOwner()
{
    State s;
    assert(BeginClick(s, true, kViewA) == Begin::kAdmitted);
    assert(!DiscardIfOwnerView(s, kViewB.view));
    assert(!DiscardIfOwnerView(s, kNoView.view));
    assert(s.held);
    assert(s.owner == kViewA);
    assert(CancelClick(s, kViewA));
}

static void TestDiscardOwnerViewIgnoresGenerationChange()
{
    State s;
    assert(BeginClick(s, true, kViewA) == Begin::kAdmitted);
    assert(DiscardIfOwnerView(s, kViewAReloaded.view));
    assert(!s.held);
    assert(s.owner == kNoView);
}

static void TestDiscardWithoutHoldIsInert()
{
    State s;
    assert(!DiscardIfOwnerView(s, kViewA.view));
    assert(!s.held);
}

static void TestReleaseWhileCursorPolicyHiddenStillReachesOwner()
{
    State s;
    assert(BeginClick(s, true, kViewA) == Begin::kAdmitted);
    assert(EndClick(s, kViewA) == End::kReleaseOwner);
    assert(!s.held);
}

static void TestCancelWhileCursorPolicyHiddenStillReleases()
{
    State s;
    assert(BeginClick(s, true, kViewA) == Begin::kAdmitted);
    assert(CancelClick(s, kViewA));
    assert(!s.held);
    assert(s.owner == kNoView);
}

static void TestOwnedSequenceConsumesHeldSamples()
{
    State s;
    assert(BeginClick(s, true, kViewA) == Begin::kAdmitted);
    assert(s.held);
    assert(s.held);
    assert(s.held);
    assert(EndClick(s, kViewA) == End::kReleaseOwner);
    assert(!s.held);
}

static void TestRejectedPressLeavesSequenceUnowned()
{
    State s;
    assert(BeginClick(s, false, kViewA) == Begin::kRejected);
    assert(!s.held);
    assert(!s.held);
    assert(EndClick(s, kViewA) == End::kNothing);
    assert(!s.held);
}

static void TestMissingReleaseThenNewSession()
{
    State s;
    assert(BeginClick(s, true, kViewA) == Begin::kAdmitted);
    ResetSession(s);
    assert(!s.held);
    assert(s.owner == kNoView);
    assert(BeginClick(s, true, kViewB) == Begin::kAdmitted);
    assert(EndClick(s, kViewB) == End::kReleaseOwner);
}

static void TestNoStaleFirstClickSuppressionAfterRecovery()
{
    State s;
    assert(BeginClick(s, true, kViewA) == Begin::kAdmitted);
    assert(!CancelClick(s, kViewB));
    assert(BeginClick(s, true, kViewB) == Begin::kAdmitted);
    assert(EndClick(s, kViewB) == End::kReleaseOwner);
    assert(BeginClick(s, true, kViewB) == Begin::kAdmitted);
}

static void TestFocusLossCancelPreventsUnmatchedRelease()
{
    State leaked;
    assert(BeginClick(leaked, true, kViewA) == Begin::kAdmitted);
    assert(BeginClick(leaked, true, kViewA) == Begin::kRejected);
    assert(EndClick(leaked, kViewA) == End::kReleaseOwner);

    State cancelled;
    assert(BeginClick(cancelled, true, kViewA) == Begin::kAdmitted);
    assert(CancelClick(cancelled, kViewA));
    assert(!cancelled.held);
    assert(cancelled.owner == kNoView);
    assert(BeginClick(cancelled, true, kViewA) == Begin::kAdmitted);
    assert(EndClick(cancelled, kViewA) == End::kReleaseOwner);
}

static void TestEndWithoutHoldIsInert()
{
    State s;
    assert(EndClick(s, kViewA) == End::kNothing);
}

static void TestMoveLogIsBoundedAndReset()
{
    State s;
    for (std::uint32_t i = 0; i < kMoveLogLimit; ++i) assert(RecordMove(s));
    assert(!RecordMove(s));
    assert(s.movesLogged == kMoveLogLimit);
    ResetSession(s);
    assert(s.movesLogged == 0);
    assert(RecordMove(s));
    assert(!s.hasSample);
}

int main()
{
    TestFocusedClickRouting();
    TestPhysicalMouseRoutingUnchanged();
    TestOwnerDerivation();
    TestDownUpSameOwner();
    TestSecondBeginWhileHeldIsRejected();
    TestRegionRejectionDoesNotBegin();
    TestInvalidOwnerDoesNotBegin();
    TestFocusLossWhileHeld();
    TestHandoffToAnotherViewWhileHeld();
    TestReloadGenerationChangeWhileHeld();
    TestDestroyWhileHeld();
    TestSameOwnerCancelReleases();
    TestCancelWithoutHoldIsInert();
    TestReleaseIntoAnotherTargetIsDiscarded();

    TestMissingReleaseThenNewSession();
    TestNoStaleFirstClickSuppressionAfterRecovery();
    TestFocusLossCancelPreventsUnmatchedRelease();
    TestReleaseOutsideRegionStillReachesOwner();
    TestCancelOutsideRegionStillReachesOwner();
    TestReleaseAfterOwnerInvalidDiscards();
    TestReloadGenerationReleaseDiscards();
    TestOwnedSequenceConsumesHeldSamples();
    TestRejectedPressLeavesSequenceUnowned();
    TestDiscardOwnerViewDropsOnlyItsOwnHold();
    TestDiscardUnrelatedViewKeepsOwner();
    TestDiscardFailedDomCleanupOfUnrelatedViewKeepsOwner();
    TestDiscardOwnerViewIgnoresGenerationChange();
    TestDiscardWithoutHoldIsInert();
    TestReleaseWhileCursorPolicyHiddenStillReachesOwner();
    TestCancelWhileCursorPolicyHiddenStillReleases();
    TestEndWithoutHoldIsInert();
    TestMoveLogIsBoundedAndReset();
    return 0;
}
