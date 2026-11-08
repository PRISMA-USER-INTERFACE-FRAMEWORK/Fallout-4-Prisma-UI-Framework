#include "PrismaUI/ViewCreateStagger.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

    using Stagger = PrismaUI::WebRuntimeUltralight::ViewCreateStagger;

    int g_failures = 0;

    void Check(bool condition, const char* what) {
        if (!condition) {
            std::fprintf(stderr, "FAIL: %s\n", what);
            ++g_failures;
        }
    }

    Stagger::Task Record(std::vector<std::string>& log, std::string name) {
        return [&log, name = std::move(name)] { log.push_back(name); };
    }

    void RunAll(Stagger& stagger, std::int64_t nowMs, bool backoff = false) {
        for (;;) {
            auto next = stagger.TakeNext(nowMs, backoff);
            if (next.step != Stagger::Step::Run) return;
            next.task();
        }
    }

    void FifoWithOpsBehindCreates() {
        Stagger stagger;
        stagger.Configure({100, 0});
        std::vector<std::string> log;
        Stagger::Task c1 = Record(log, "create1"), o1 = Record(log, "op1a"), c2 = Record(log, "create2"),
                      o2 = Record(log, "op2a"), o1b = Record(log, "op1b");
        Check(stagger.PushCreate(1, c1), "create 1 staged");
        Check(stagger.PushOp(1, o1), "op for staged view 1 queued");
        Check(stagger.PushCreate(2, c2), "create 2 staged");
        Check(stagger.PushOp(2, o2), "op for staged view 2 queued");
        Check(stagger.PushOp(1, o1b), "later op for view 1 queued behind create 2");
        RunAll(stagger, 0);
        const std::vector<std::string> expected{"create1", "op1a", "create2", "op2a", "op1b"};
        Check(log == expected, "strict FIFO across views, ops stay behind their create");
        Check(!stagger.HasPending(), "queue empty after drain");
    }

    void PushRules() {
        Stagger stagger;
        std::vector<std::string> log;
        Stagger::Task a = Record(log, "a"), dup = Record(log, "dup"), op = Record(log, "op");
        Check(stagger.PushCreate(5, a), "first create admitted");
        Check(!stagger.PushCreate(5, dup), "second create for the same view refused");
        Check(dup != nullptr, "refused create keeps its task");
        Check(!stagger.PushOp(9, op), "op for an unstaged view refused");
        Check(op != nullptr, "refused op keeps its task");
        Check(stagger.IsStaged(5) && !stagger.IsStaged(9), "staged set tracks creates only");
    }

    void WindowBudget() {
        Stagger stagger;
        stagger.Configure({1, 0});
        std::vector<std::string> log;
        Stagger::Task a = Record(log, "a"), b = Record(log, "b");
        stagger.PushCreate(1, a);
        stagger.PushCreate(2, b);
        auto first = stagger.TakeNext(0, false);
        Check(first.step == Stagger::Step::Run, "first create runs at window start");
        first.task();
        auto blocked = stagger.TakeNext(5, false);
        Check(blocked.step == Stagger::Step::Wait, "second create waits within the same window");
        Check(blocked.waitMs == Stagger::kWindowMs - 5, "wait reaches the window end");
        auto next = stagger.TakeNext(Stagger::kWindowMs, false);
        Check(next.step == Stagger::Step::Run, "next window admits the next create");
        next.task();
        const std::vector<std::string> expected{"a", "b"};
        Check(log == expected, "budget keeps creates in order");
    }

    void MinGap() {
        Stagger stagger;
        stagger.Configure({10, 5});
        std::vector<std::string> log;
        Stagger::Task a = Record(log, "a"), b = Record(log, "b");
        stagger.PushCreate(1, a);
        stagger.PushCreate(2, b);
        auto first = stagger.TakeNext(0, false);
        first.task();
        auto early = stagger.TakeNext(3, false);
        Check(early.step == Stagger::Step::Wait && early.waitMs == 2, "min gap holds the next create");
        auto due = stagger.TakeNext(5, false);
        Check(due.step == Stagger::Step::Run, "min gap elapsed admits the next create");
    }

    void OpsDoNotConsumeBudget() {
        Stagger stagger;
        stagger.Configure({1, 0});
        std::vector<std::string> log;
        Stagger::Task a = Record(log, "a"), opA = Record(log, "opA");
        stagger.PushCreate(1, a);
        stagger.PushOp(1, opA);
        RunAll(stagger, 0);
        const std::vector<std::string> expected{"a", "opA"};
        Check(log == expected, "op behind a create runs without spending the window budget");
    }

    void BackoffBlocksCreatesOnly() {
        Stagger stagger;
        stagger.Configure({1, 0});
        std::vector<std::string> log;
        Stagger::Task a = Record(log, "a");
        stagger.PushCreate(1, a);
        auto held = stagger.TakeNext(0, true);
        Check(held.step == Stagger::Step::Wait && held.waitMs == Stagger::kBackoffPollMs, "backoff holds the create");
        auto released = stagger.TakeNext(1, false);
        Check(released.step == Stagger::Step::Run, "backoff release admits the create");
    }

    void CancelDropsCreateAndOps() {
        Stagger stagger;
        stagger.Configure({1, 0});
        std::vector<std::string> log;
        Stagger::Task a = Record(log, "a"), b = Record(log, "b"), opA = Record(log, "opA"), opB = Record(log, "opB");
        stagger.PushCreate(1, a);
        stagger.PushOp(1, opA);
        stagger.PushCreate(2, b);
        stagger.PushOp(2, opB);
        auto dropped = stagger.Cancel(1);
        Check(dropped.size() == 2, "cancel returns the create and its op");
        Check(!stagger.IsStaged(1), "canceled view is no longer staged");
        Check(!stagger.PushOp(1, opA), "op for a canceled view refused");
        dropped.clear();
        RunAll(stagger, 0);
        const std::vector<std::string> expected{"b", "opB"};
        Check(log == expected, "canceled entries never run and other views are unaffected");
    }

    void CloseDrainsAndRefusesLaterPushes() {
        Stagger stagger;
        std::vector<std::string> log;
        Stagger::Task a = Record(log, "a"), opA = Record(log, "opA");
        stagger.PushCreate(1, a);
        stagger.PushOp(1, opA);
        auto dropped = stagger.Close();
        Check(dropped.size() == 2 && !stagger.HasPending() && !stagger.IsStaged(1), "close empties the queue");
        Check(log.empty(), "close does not run tasks");
        Stagger::Task late = Record(log, "late"), lateOp = Record(log, "lateOp");
        Check(!stagger.PushCreate(2, late), "a create pushed after close is refused, not stranded");
        Check(late != nullptr, "a refused create keeps its task for the caller");
        Check(!stagger.PushOp(1, lateOp), "an op pushed after close is refused");
        Check(!stagger.HasPending(), "nothing is queued after close");
    }

    void OpsStayStagedUntilViewDrains() {
        Stagger stagger;
        stagger.Configure({1, 0});
        std::vector<std::string> log;
        Stagger::Task c1 = Record(log, "create1"), o1a = Record(log, "op1a"), c2 = Record(log, "create2"),
                      o1b = Record(log, "op1b");
        stagger.PushCreate(1, c1);
        stagger.PushOp(1, o1a);
        stagger.PushCreate(2, c2);
        auto first = stagger.TakeNext(0, false);
        Check(first.step == Stagger::Step::Run, "view 1 create runs first");
        first.task();
        Check(stagger.IsStaged(1), "view 1 stays staged while its op is queued behind create 2");
        Check(stagger.PushOp(1, o1b), "later op for view 1 is accepted and stays behind op1a");
        RunAll(stagger, 100);
        const std::vector<std::string> expected{"create1", "op1a", "create2", "op1b"};
        Check(log.size() == 4 && log == expected, "ops for a view never overtake earlier staged ops");
        Check(!stagger.IsStaged(1) && !stagger.IsStaged(2), "views unstage after their last entry drains");
    }

}

int main() {
    FifoWithOpsBehindCreates();
    PushRules();
    WindowBudget();
    MinGap();
    OpsDoNotConsumeBudget();
    BackoffBlocksCreatesOnly();
    CancelDropsCreateAndOps();
    CloseDrainsAndRefusesLaterPushes();
    OpsStayStagedUntilViewDrains();
    if (g_failures) {
        std::fprintf(stderr, "view_create_stagger_tests: %d failure(s)\n", g_failures);
        return EXIT_FAILURE;
    }
    std::puts("view_create_stagger_tests passed");
    return EXIT_SUCCESS;
}
