#include "PrismaUI/PresentationHeartbeatPolicy.h"
#include "PrismaUI/PresentHoldPolicy.h"

#include <cstdio>

using namespace PrismaUI::PresentationHeartbeatPolicy;

static int failures = 0;

static void expect(bool condition, const char* message) {
    std::printf("[%s] %s\n", condition ? "PASS" : "FAIL", message);
    if (!condition) ++failures;
}

static void testUnrelatedCommittedViewDoesNotRefreshFocus() {
    constexpr std::int64_t threshold = 2000;
    const std::vector<PrismaUI::PresentHold::ViewSource> visible{{1, false, true}, {2, false, false}};
    const std::vector<std::uint64_t> committed{1};
    const auto classification = PrismaUI::PresentHold::Classify(visible, committed);
    PrismaUI::PresentHold::Policy present;
    std::int64_t globalHeartbeat = 0;
    constexpr std::uint64_t focused = 2;

    for (const std::int64_t nowMs : {3000LL, 3500LL, 4000LL, 4500LL}) {
        const auto decision = present.OnPresent(nowMs, true, classification.complete,
                                                classification.committedFaithful, true, false);
        expect(
            decision.action == PrismaUI::PresentHold::Action::BlitCommitted,
            "A-only committed frame remains eligible for held-frame blit while B has no source");
        if (std::find(committed.begin(), committed.end(), focused) != committed.end()) globalHeartbeat = nowMs;
    }

    const Inputs focusedB{true, true, true, 5001, 2500, globalHeartbeat};
    expect(
        ShouldEmergencyReleaseFocus(focusedB, threshold),
        "unrelated A-only successful blits must not keep focused B alive");
}

static void testFocusedCommittedViewRefreshesHeartbeat() {
    constexpr std::int64_t threshold = 2000;
    const std::vector<PrismaUI::PresentHold::ViewSource> visible{{1, false, true}};
    const std::vector<std::uint64_t> committed{1};
    const auto classification = PrismaUI::PresentHold::Classify(visible, committed);
    PrismaUI::PresentHold::Policy present;
    std::int64_t heartbeat = 0;

    for (const std::int64_t nowMs : {3000LL, 3500LL, 4000LL, 4500LL}) {
        const auto decision = present.OnPresent(nowMs, true, classification.complete,
                                                classification.committedFaithful, true, false);
        expect(decision.action == PrismaUI::PresentHold::Action::BlitCommitted,
               "focused committed frame remains eligible for repeated held-frame blits");
        heartbeat = nowMs;
    }

    const Inputs focusedA{true, true, true, 5001, 2500, heartbeat};
    expect(!ShouldEmergencyReleaseFocus(focusedA, threshold),
           "repeated successful held-frame blits refresh the focused heartbeat");
}

static void testSuccessfulBlitUsesCompletionTime() {
    constexpr std::int64_t threshold = 2000;
    const Inputs completionTimestamp{true, true, true, 3501, 1000, 3501};
    expect(!ShouldEmergencyReleaseFocus(completionTimestamp, threshold),
           "successful final blit completion time keeps focus fresh");

    const Inputs staleTimestamp{true, true, true, 3501, 1000, 1000};
    expect(ShouldEmergencyReleaseFocus(staleTimestamp, threshold),
           "a pre-draw timestamp can falsely age a long successful presentation");
}

int main() {
    testUnrelatedCommittedViewDoesNotRefreshFocus();
    testFocusedCommittedViewRefreshesHeartbeat();
    testSuccessfulBlitUsesCompletionTime();
    constexpr std::int64_t threshold = 2000;

    Inputs staticPanel;
    staticPanel.viewLive = true;
    staticPanel.focusSession = true;
    staticPanel.presentAlive = true;
    staticPanel.nowMs = 10000;
    staticPanel.focusEpisodeStartMs = 6000;
    staticPanel.lastSuccessfulPresentationMs = 9999;
    expect(!ShouldEmergencyReleaseFocus(staticPanel, threshold),
           "static retained frame stays focused while presentation succeeds");

    Inputs stalePresentation = staticPanel;
    stalePresentation.lastSuccessfulPresentationMs = 7000;
    expect(ShouldEmergencyReleaseFocus(stalePresentation, threshold),
           "live focused view releases only when successful presentation is actually stale");

    Inputs neverPresented = staticPanel;
    neverPresented.focusEpisodeStartMs = 7000;
    neverPresented.lastSuccessfulPresentationMs = 0;
    expect(ShouldEmergencyReleaseFocus(neverPresented, threshold),
           "a live focused view that never presents still times out from focus start");

    Inputs freshNeverPresented = neverPresented;
    freshNeverPresented.focusEpisodeStartMs = 9000;
    expect(!ShouldEmergencyReleaseFocus(freshNeverPresented, threshold),
           "a newly focused view gets the full presentation grace window");

    Inputs presentStopped = stalePresentation;
    presentStopped.presentAlive = false;
    expect(!ShouldEmergencyReleaseFocus(presentStopped, threshold),
           "whole-Present stalls remain owned by the independent Present watchdog");

    Inputs notFocused = stalePresentation;
    notFocused.focusSession = false;
    expect(!ShouldEmergencyReleaseFocus(notFocused, threshold),
           "unfocused views are never released by the focused presentation watchdog");

    Inputs notLive = stalePresentation;
    notLive.viewLive = false;
    expect(!ShouldEmergencyReleaseFocus(notLive, threshold),
           "non-live states are handled by the existing health recovery path");

    Inputs noHistory;
    noHistory.viewLive = true;
    noHistory.focusSession = true;
    noHistory.presentAlive = true;
    noHistory.nowMs = 10000;
    expect(!ShouldEmergencyReleaseFocus(noHistory, threshold),
           "missing focus and presentation history does not fabricate a stale timestamp");

    if (failures == 0) {
        std::puts("All presentation heartbeat policy assertions passed.");
        return 0;
    }
    std::printf("%d presentation heartbeat assertion(s) failed.\n", failures);
    return 1;
}
