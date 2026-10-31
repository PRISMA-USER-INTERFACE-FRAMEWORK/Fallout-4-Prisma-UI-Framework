#include "PrismaUI/ModelPreviewShutdownGate.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

using PrismaUI::ModelPreview::ShutdownGate;

static int g_failures = 0;

static void expect(bool cond, const char* what)
{
    std::printf("  [%s] %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) ++g_failures;
}

static void waitFor(const std::atomic<bool>& flag)
{
    while (!flag.load()) std::this_thread::yield();
}

static void testShutdownWaitsForInFlightTick()
{
    ShutdownGate gate;
    std::atomic<bool> tickEntered{false};
    std::atomic<bool> releaseTick{false};
    std::atomic<bool> tickFinished{false};
    std::atomic<bool> shutdownAttempted{false};
    std::atomic<bool> shutdownReturned{false};
    bool tickOwned = false;
    bool finishedBeforeShutdown = false;

    std::thread tick([&] {
        auto lock = gate.EnterTick();
        tickOwned = lock.owns_lock();
        tickEntered.store(true);
        if (!tickOwned) return;
        waitFor(releaseTick);
        tickFinished.store(true);
    });
    waitFor(tickEntered);

    std::thread shutdown([&] {
        shutdownAttempted.store(true);
        auto lock = gate.BeginShutdown();
        finishedBeforeShutdown = tickFinished.load();
        shutdownReturned.store(true);
    });
    waitFor(shutdownAttempted);
    for (int i = 0; i < 1000 && !shutdownReturned.load(); ++i) std::this_thread::yield();
    const bool blockedWhileTickHeld = !shutdownReturned.load();

    releaseTick.store(true);
    tick.join();
    shutdown.join();

    expect(tickOwned, "a tick enters the gate when shutdown has not begun");
    expect(blockedWhileTickHeld, "BeginShutdown does not return while a tick holds the gate");
    expect(finishedBeforeShutdown, "BeginShutdown acquires only after the in-flight tick finished");
}

static void testNoTickBodyRunsAfterShutdownBegins()
{
    int overlaps = 0;
    int lateTicks = 0;
    int admittedAfter = 0;
    for (int round = 0; round < 300; ++round) {
        ShutdownGate gate;
        std::atomic<bool> stop{false};
        std::atomic<bool> shutdownActive{false};
        std::atomic<bool> shutdownDone{false};
        std::atomic<int> attempts{0};
        std::atomic<int> roundOverlaps{0};
        std::atomic<int> roundLate{0};

        std::thread tick([&] {
            while (!stop.load()) {
                attempts.fetch_add(1);
                auto lock = gate.EnterTick();
                if (!lock.owns_lock()) continue;
                if (shutdownActive.load()) roundOverlaps.fetch_add(1);
                if (shutdownDone.load()) roundLate.fetch_add(1);
            }
        });
        while (attempts.load() < 2) std::this_thread::yield();

        {
            auto lock = gate.BeginShutdown();
            shutdownActive.store(true);
            for (int i = 0; i < 10; ++i) std::this_thread::yield();
            shutdownActive.store(false);
            shutdownDone.store(true);
        }
        stop.store(true);
        tick.join();

        overlaps += roundOverlaps.load();
        lateTicks += roundLate.load();
        if (gate.EnterTick().owns_lock()) ++admittedAfter;
    }
    expect(overlaps == 0, "no tick body overlaps the shutdown work");
    expect(lateTicks == 0, "no tick body runs after shutdown completed");
    expect(admittedAfter == 0, "a tick after shutdown is skipped");
}

static void testShutdownIsIdempotentAndReentrantSafe()
{
    ShutdownGate gate;
    {
        auto first = gate.BeginShutdown();
        expect(first.owns_lock(), "the first BeginShutdown takes the gate");
        auto reentrant = gate.EnterTick();
        expect(!reentrant.owns_lock(), "a tick re-entered on the shutdown thread skips without locking");
    }
    {
        auto second = gate.BeginShutdown();
        expect(second.owns_lock(), "a repeated BeginShutdown completes without deadlock");
    }
    auto tick = gate.EnterTick();
    expect(!tick.owns_lock(), "no tick is admitted after repeated shutdown");
}

static void testTickSkipsWhileAnotherTickHoldsGate()
{
    ShutdownGate gate;
    auto held = gate.EnterTick();
    std::atomic<bool> otherOwned{true};
    std::atomic<bool> otherReturned{false};
    std::thread other([&] {
        otherOwned.store(gate.EnterTick().owns_lock());
        otherReturned.store(true);
    });
    for (int i = 0; i < 200 && !otherReturned.load(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    const bool returnedWhileHeld = otherReturned.load();
    held.unlock();
    other.join();
    expect(returnedWhileHeld, "a tick that finds the gate held returns instead of blocking");
    expect(!otherOwned.load(), "a tick that finds the gate held skips without owning it");
}

int main()
{
    testShutdownWaitsForInFlightTick();
    testNoTickBodyRunsAfterShutdownBegins();
    testShutdownIsIdempotentAndReentrantSafe();
    testTickSkipsWhileAnotherTickHoldsGate();
    if (g_failures) {
        std::printf("modelpreview shutdown gate: %d failure(s)\n", g_failures);
        return 1;
    }
    std::puts("modelpreview shutdown gate tests passed");
    return 0;
}
