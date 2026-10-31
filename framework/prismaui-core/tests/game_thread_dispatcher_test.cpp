// Generation/queue contract for GameThreadDispatcher.
//
// If Dispatch returns false, that callback must never execute later. Tasks do not survive across
// window-attachment generations. Metadata (view, generation) is kept until immediately before each
// invoke so Destroy/detach/fail-closed during a wake still cancel the rest of the batch.
//
// Build: g++ -std=c++23 -O1 -g -Wall -Wextra -Wpedantic -fsanitize=address,undefined -pthread -Isrc
//          tests/game_thread_dispatcher_test.cpp -o game_thread_dispatcher_test
// Exit code: 0 = all pass.

#include "PrismaUI/GameThreadDispatchQueue.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <thread>

using PrismaUI::GameThreadDispatchQueue::kMaxTasksPerWake;
using PrismaUI::GameThreadDispatchQueue::Queue;
using PrismaUI::GameThreadDispatchQueue::Rejection;

static int g_failures = 0;

static void expect(bool cond, const char* what)
{
    std::printf("  [%s] %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) ++g_failures;
}

static void runWake(Queue& q, const std::function<bool(uint64_t)>& viewLive,
                    std::size_t budget = kMaxTasksPerWake)
{
    q.beginDrain();
    std::size_t n = 0;
    while (n < budget) {
        auto task = q.takeNext(viewLive);
        if (!task) break;
        ++n;
        if (task->fn) task->fn();
        q.finishExecuting();
    }
    q.endDrain();
}

static void runWake(Queue& q, std::size_t budget = kMaxTasksPerWake)
{
    runWake(q, [](uint64_t) { return true; }, budget);
}

int main()
{
    std::printf("Game-thread dispatch queue generation contract\n\n");

    {
        Queue q;
        int ran = 0;
        const auto r = q.tryEnqueue([&] { ++ran; });
        expect(!r.accepted && !r.needsWake, "Dispatch while !ready returns false and does not wake");
        q.attach();
        runWake(q);
        expect(ran == 0, "a rejected !ready dispatch never runs after later Attach");
    }

    {
        Queue q;
        q.attach();
        int ran = 0;
        const auto r = q.tryEnqueue([&] { ++ran; });
        expect(r.accepted && r.needsWake, "first live enqueue is accepted and needs one wake");
        const auto r2 = q.tryEnqueue([&] { ++ran; });
        expect(r2.accepted && !r2.needsWake, "second enqueue coalesces; no extra wake");
        runWake(q);
        expect(ran == 2, "both coalesced tasks run");
        expect(q.takeWakeRearm() == false, "empty queue after drain does not rearm");
    }

    {
        Queue q;
        q.attach();
        int ran = 0;
        expect(q.tryEnqueue([&] { ++ran; }).accepted, "live enqueue accepted");
        q.detach();
        expect(!q.ready() && !q.failed(), "detach is recoverable, not fail-closed");
        q.attach();
        runWake(q);
        expect(ran == 0, "reattach does not inherit the previous generation's callbacks");
    }

    {
        Queue q;
        q.attach();
        int ran = 0;
        expect(q.tryEnqueue([&] { ++ran; }).accepted, "enqueue before wake failure");
        q.noteWakeFailure();
        expect(q.failed() && !q.ready(), "wake failure fail-closes the session");
        runWake(q);
        expect(ran == 0, "wake-failure must not leave the callback queued");
        expect(!q.tryEnqueue([&] { ++ran; }).accepted, "fail-closed rejects further dispatch");
        q.attach();
        expect(!q.tryEnqueue([&] { ++ran; }).accepted, "fail-closed cannot be revived by a plain attach");
        expect(ran == 0, "fail-closed callbacks never run later");
    }

    {
        Queue q;
        q.attach();
        int ranOld = 0;
        int ranNew = 0;
        expect(q.tryEnqueue([&] { ++ranOld; }).accepted, "enqueue before failure");
        q.failClosed();
        expect(q.tryEnqueue([&] { ++ranOld; }).reason == Rejection::Failed, "rejection while failed reports Failed");
        q.recoverAfterFailure();
        expect(!q.failed() && !q.ready(), "recovery clears the failure but still requires a verified attach");
        expect(q.tryEnqueue([&] { ++ranNew; }).reason == Rejection::NotReady, "recovered but unattached reports NotReady");
        q.attach();
        const auto r = q.tryEnqueue([&] { ++ranNew; });
        expect(r.accepted && r.reason == Rejection::None, "verified re-attach after failure accepts new work");
        runWake(q);
        expect(ranNew == 1 && ranOld == 0, "re-attach runs only new-generation work; failed-generation work never runs");
    }

    {
        Queue q;
        expect(q.tryEnqueue([] {}).reason == Rejection::NotReady, "rejection before attach reports NotReady");
        q.attach();
        for (std::size_t i = 0; i < PrismaUI::GameThreadDispatchQueue::kMaxQueue; ++i) (void)q.tryEnqueue([] {});
        expect(q.tryEnqueue([] {}).reason == Rejection::QueueFull, "rejection at capacity reports QueueFull");
    }

    {
        Queue q;
        q.attach();
        int ranKeep = 0;
        int ranDrop = 0;
        expect(q.tryEnqueue([&] { ++ranKeep; }, 1).accepted, "view-scoped keep enqueue");
        expect(q.tryEnqueue([&] { ++ranDrop; }, 2).accepted, "view-scoped drop enqueue");
        q.dropView(2);
        runWake(q);
        expect(ranKeep == 1 && ranDrop == 0, "DestroyView drops only that view's queued events");
    }

    {
        Queue q;
        q.attach();
        int ran = 0;
        expect(q.tryEnqueue([&] { ++ran; }, 7).accepted, "enqueue for a view that dies before drain");
        runWake(q, [](uint64_t) { return false; });
        expect(ran == 0, "takeNext skips callbacks whose view is no longer live");
    }

    {
        Queue q;
        q.attach();
        int ranB = 0;
        expect(q.tryEnqueue([&] { q.dropView(42); }, 42).accepted, "A enqueued");
        expect(q.tryEnqueue([&] { ++ranB; }, 42).accepted, "B enqueued for same view");
        runWake(q);
        expect(ranB == 0, "A destroys its own view -> B for same view never executes");
    }

    {
        Queue q;
        q.attach();
        int ranB = 0;
        expect(q.tryEnqueue([&] { q.detach(); }).accepted, "A detaches");
        expect(q.tryEnqueue([&] { ++ranB; }).accepted, "B from previous generation");
        runWake(q);
        expect(ranB == 0, "A detaches dispatcher -> B from previous generation never executes");
    }

    {
        Queue q;
        q.attach();
        int ranB = 0;
        expect(q.tryEnqueue([&] { q.failClosed(); }).accepted, "A fail-closes");
        expect(q.tryEnqueue([&] { ++ranB; }).accepted, "B after fail-close");
        runWake(q);
        expect(ranB == 0, "A fail-closes dispatcher -> B never executes");
    }

    {
        Queue q;
        q.attach();
        int ran = 0;
        bool allAccepted = true;
        for (std::size_t i = 0; i < kMaxTasksPerWake + 8; ++i) {
            if (!q.tryEnqueue([&] { ++ran; }).accepted) allAccepted = false;
        }
        expect(allAccepted, "budget enqueue");
        runWake(q, kMaxTasksPerWake);
        expect(ran == static_cast<int>(kMaxTasksPerWake), "one wake runs at most kMaxTasksPerWake tasks");
        expect(q.takeWakeRearm(), "remaining work rearms a follow-up wake");
        runWake(q, kMaxTasksPerWake);
        expect(ran == static_cast<int>(kMaxTasksPerWake + 8), "follow-up wake finishes the rest");
    }

    {
        Queue q;
        q.attach();
        std::atomic<int> phase{0};
        std::atomic<bool> waited{false};
        expect(q.tryEnqueue([&] {
            phase.store(1);
            while (phase.load() != 2) std::this_thread::yield();
        }, 42).accepted, "in-flight enqueue");

        std::thread runner([&] { runWake(q); });
        while (!q.isExecuting(42)) std::this_thread::yield();

        std::thread waiter([&] {
            q.waitUntilNotExecuting(42);
            waited.store(true);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        expect(!waited.load(), "waitUntilNotExecuting blocks while the view callback is in flight");
        phase.store(2);
        runner.join();
        waiter.join();
        expect(waited.load(), "waitUntilNotExecuting returns after finishExecuting");
    }

    {
        Queue q;
        q.attach();
        std::atomic<int> phase{0};
        expect(q.tryEnqueue([&] {
            phase.store(1);
            while (phase.load() != 2) std::this_thread::yield();
        }, 42).accepted, "timeout-probe enqueue");
        std::thread runner([&] { runWake(q); });
        while (!q.isExecuting(42)) std::this_thread::yield();
        expect(!q.waitUntilNotExecutingFor(42, std::chrono::milliseconds(20)),
               "waitUntilNotExecutingFor times out while the callback is still running");
        phase.store(2);
        runner.join();
        expect(q.waitUntilNotExecutingFor(42, std::chrono::milliseconds(20)),
               "waitUntilNotExecutingFor succeeds after finishExecuting");
    }

    {
        Queue q;
        q.attach();
        std::size_t accepted = 0;
        for (std::size_t i = 0; i < PrismaUI::GameThreadDispatchQueue::kMaxQueue + 8; ++i) {
            if (q.tryEnqueue([] {}).accepted) ++accepted;
        }
        expect(accepted == PrismaUI::GameThreadDispatchQueue::kMaxQueue,
               "public queue has a backpressure ceiling");
        expect(!q.tryEnqueue([] {}).accepted, "overflow returns false and does not retain");
        expect(q.tryEnqueue([] {}, 0, true).accepted, "safety enqueue uses the reserved slot when the queue is full");
        expect(!q.tryEnqueue([] {}, 0, true).accepted, "only one reserved safety slot");
    }

    {
        Queue q;
        q.attach();
        q.dropView(42);
        expect(!q.tryEnqueue([] {}, 42).accepted, "late producer after DropView is rejected");
        expect(q.isCancelled(42), "DropView tombstones the view");
        expect(q.tryEnqueue([] {}, 43).accepted, "other views still enqueue after a tombstone");
    }

    {
        Queue q;
        q.attach();
        std::atomic<int> phase{0};
        int ranLate = 0;
        expect(q.tryEnqueue([&] {
            phase.store(1);
            while (phase.load() != 2) std::this_thread::yield();
        }, 42).accepted, "in-flight plus late-producer enqueue");
        std::thread runner([&] { runWake(q); });
        while (!q.isExecuting(42)) std::this_thread::yield();
        std::thread dropper([&] { q.dropView(42); });
        while (!q.isCancelled(42)) std::this_thread::yield();
        expect(!q.tryEnqueue([&] { ++ranLate; }, 42).accepted,
               "late producer during DropView wait cannot enqueue");
        phase.store(2);
        runner.join();
        dropper.join();
        expect(ranLate == 0, "late producer after in-flight DropView never runs");
        expect(!q.tryEnqueue([] {}, 42).accepted, "tombstone outlives the in-flight callback");
    }

    {
        Queue q;
        q.attach();
        int ran = 0;
        expect(q.tryEnqueue([&] { ++ran; }).accepted, "nested-drain A");
        expect(q.tryEnqueue([&] { ++ran; }).accepted, "nested-drain B");
        q.beginDrain();
        auto a = q.takeNext([](uint64_t) { return true; });
        expect(static_cast<bool>(a), "outer drain took A");
        expect(!q.takeNext([](uint64_t) { return true; }),
               "reentrant takeNext refuses to start B while A is executing");
        const auto nested = q.tryEnqueue([&] { ++ran; });
        expect(nested.accepted && !nested.needsWake,
               "producers during an active drain do not post another wake");
        if (a && a->fn) a->fn();
        q.finishExecuting();
        auto b = q.takeNext([](uint64_t) { return true; });
        expect(static_cast<bool>(b), "outer drain resumes B after A finishes");
        if (b && b->fn) b->fn();
        q.finishExecuting();
        auto c = q.takeNext([](uint64_t) { return true; });
        expect(static_cast<bool>(c), "work enqueued during drain is consumed by the same drain");
        if (c && c->fn) c->fn();
        q.finishExecuting();
        q.endDrain();
        expect(ran == 3, "nested-drain session ran A, B, and the intra-drain enqueue");
        expect(!q.takeWakeRearm(), "empty queue after a completed drain does not rearm");
    }

    if (g_failures == 0) {
        std::printf("\nall dispatcher generation checks passed\n");
        return 0;
    }
    std::printf("\n%d dispatcher generation check(s) failed\n", g_failures);
    return 1;
}
