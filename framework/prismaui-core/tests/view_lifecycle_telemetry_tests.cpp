#include "PrismaUI/ViewLifecycleBurst.h"

#include <cstdio>
#include <cstdlib>

namespace {

    int g_failures = 0;

    void Check(bool condition, const char* what) {
        if (!condition) {
            std::fprintf(stderr, "FAIL: %s\n", what);
            ++g_failures;
        }
    }

    void EmptyBurstIsReportableAndZero() {
        PrismaUI::ViewLifecycle::Burst burst;
        Check(!burst.Open(), "a new burst is closed");
        Check(burst.ReadyToClose(0, 10'000'000), "a closed burst is reportable so the burst sample keeps firing");
        const auto s = burst.Take();
        Check(s.views == 0 && s.domReady == 0 && s.createSamples == 0, "closed burst summarises to zeros");
    }

    void CountsRequestsAndWaitsForDomReady() {
        PrismaUI::ViewLifecycle::Burst burst;
        Check(burst.Request(1, 1000) == 1, "first request is burst size 1");
        Check(burst.Request(2, 2000) == 2, "second request is burst size 2");
        Check(!burst.ReadyToClose(3000, 10'000'000), "outstanding views hold the burst open");
        burst.DomReady(1, 5000);
        Check(!burst.ReadyToClose(6000, 10'000'000), "one view still outstanding");
        burst.DomReady(2, 9000);
        Check(burst.ReadyToClose(9500, 10'000'000), "all DOM-ready closes the burst");
        const auto s = burst.Take();
        Check(s.views == 2 && s.domReady == 2, "summary counts requests and DOM-ready");
        Check(s.firstRequestUs == 1000 && s.lastDomReadyUs == 9000, "summary spans first request to last DOM-ready");
        Check(s.outstanding == 0, "nothing outstanding at close");
        Check(!burst.Open(), "Take closes the burst");
    }

    void CreateDurationsAndAbandonedViews() {
        PrismaUI::ViewLifecycle::Burst burst;
        burst.Request(10, 0);
        burst.Request(11, 0);
        burst.Request(12, 0);
        burst.CreateDone(100);
        burst.CreateDone(300);
        burst.CreateDone(200);
        burst.Abandon(11);
        burst.DomReady(10, 50);
        burst.DomReady(12, 60);
        const auto s = burst.Take();
        Check(s.createSamples == 3, "three create samples");
        Check(s.createTotalUs == 600 && s.createMaxUs == 300, "create total and max");
        Check(s.MeanCreateUs() == 200.0, "create mean");
        Check(s.abandoned == 1, "abandoned view is counted once");
        Check(s.domReady == 2, "DOM-ready excludes the abandoned view");
    }

    void DuplicateAndUnknownEventsAreIgnored() {
        PrismaUI::ViewLifecycle::Burst burst;
        burst.Request(7, 0);
        burst.DomReady(7, 10);
        burst.DomReady(7, 20);
        burst.DomReady(99, 30);
        burst.Abandon(7);
        const auto s = burst.Take();
        Check(s.domReady == 1 && s.lastDomReadyUs == 10, "a second DOM-ready for one view is ignored");
        Check(s.abandoned == 0, "abandon after DOM-ready does not count");
    }

    void TimeoutReportsOutstandingAndDropsLateEvents() {
        PrismaUI::ViewLifecycle::Burst burst;
        burst.Request(1, 0);
        burst.Request(2, 0);
        burst.DomReady(1, 100);
        Check(!burst.ReadyToClose(9'999'999, 10'000'000), "not yet at the cap");
        Check(burst.ReadyToClose(10'000'000, 10'000'000), "cap releases an outstanding burst");
        const auto s = burst.Take();
        Check(s.outstanding == 1, "timed-out burst reports the outstanding view");
        burst.DomReady(2, 20'000'000);
        Check(!burst.Open(), "a late DOM-ready does not reopen the burst");
        burst.Request(3, 30'000'000);
        const auto next = burst.Take();
        Check(next.views == 1 && next.domReady == 0, "a late DOM-ready is not attributed to the next burst");
    }

    void IdleGapStartsNewBurst() {
        PrismaUI::ViewLifecycle::Burst burst;
        Check(burst.Request(10, 0) == 1, "first request opens a burst");
        Check(burst.Request(11, 1'000'000) == 2, "a request inside the idle window extends the burst");
        Check(burst.Request(12, 7'000'000) == 1, "a request after an idle gap starts a new burst");
        burst.DomReady(10, 7'100'000);
        burst.DomReady(12, 7'200'000);
        const auto s = burst.Take();
        Check(s.views == 1 && s.domReady == 1, "the old burst's pending view does not carry into the new burst");
        Check(s.outstanding == 0, "nothing outstanding after the new burst's DOM-ready");
    }

    void IdleGapWithNoTakeDoesNotGrowForever() {
        PrismaUI::ViewLifecycle::Burst burst;
        std::size_t last = 0;
        for (std::uint64_t i = 0; i < 50; ++i) {
            last = burst.Request(i, static_cast<std::int64_t>(i) * 6'000'000);
            burst.DomReady(i, static_cast<std::int64_t>(i) * 6'000'000 + 1);
        }
        Check(last == 1, "a burst that is never taken still restarts after each idle gap");
    }

}

int main() {
    EmptyBurstIsReportableAndZero();
    CountsRequestsAndWaitsForDomReady();
    CreateDurationsAndAbandonedViews();
    DuplicateAndUnknownEventsAreIgnored();
    TimeoutReportsOutstandingAndDropsLateEvents();
    IdleGapStartsNewBurst();
    IdleGapWithNoTakeDoesNotGrowForever();
    if (g_failures) {
        std::fprintf(stderr, "view_lifecycle_telemetry_tests: %d failure(s)\n", g_failures);
        return EXIT_FAILURE;
    }
    std::puts("view_lifecycle_telemetry_tests passed");
    return EXIT_SUCCESS;
}
