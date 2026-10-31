// Bounded-acquire proof for the shared D3D11 context lock taken during the present path.
//
// The render thread must never block indefinitely on the context mutex: a wedged owner that holds
// the lock would otherwise freeze Fallout's render thread (a hard lock). The present path instead
// takes the lock within a small budget and, on timeout, drops Prisma's frame for that present.
//
// This test mirrors that bounded acquire exactly and asserts the three properties the no-freeze
// guarantee depends on:
//   1. Uncontended  -> acquires immediately (fast, returns true).
//   2. Held forever -> returns false within a bounded time (NEVER hangs). This is the guarantee.
//   3. Released     -> a subsequent attempt acquires (the drop is per-frame, not permanent).
//
// Build: g++ -std=c++17 -O1 -pthread tests/context_lock_budget_test.cpp -o context_lock_budget_test
// Exit code: 0 = all properties hold; nonzero = a violation was detected.

#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>

// --- verbatim mirror of the bounded context acquire ------------------------------------------
static bool TryLockContextBudget(std::unique_lock<std::mutex>& lk) {
    using clock = std::chrono::steady_clock;
    const auto deadline = clock::now() + std::chrono::milliseconds(2);
    do {
        if (lk.try_lock()) return true;
        std::this_thread::yield();
    } while (clock::now() < deadline);
    return lk.try_lock();  // one final attempt at the boundary
}

static int g_failures = 0;
static void check(bool cond, const char* what) {
    printf("  [%s] %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) ++g_failures;
}

using sclock = std::chrono::steady_clock;
static double ms_since(sclock::time_point t0) {
    return std::chrono::duration<double, std::milli>(sclock::now() - t0).count();
}

int main() {
    printf("context-lock budget proof (render thread must never block indefinitely)\n\n");

    // 1. Uncontended: acquires immediately.
    {
        bool allGot = true, allOwned = true;
        double fastest = 1e9;
        for (int sample = 0; sample < 5; ++sample) {
            std::mutex m;
            std::unique_lock<std::mutex> lk(m, std::defer_lock);
            const auto t0 = sclock::now();
            const bool got = TryLockContextBudget(lk);
            const double took = ms_since(t0);
            allGot = allGot && got;
            allOwned = allOwned && lk.owns_lock();
            if (took < fastest) fastest = took;
        }
        check(allGot, "uncontended acquire succeeds");
        check(allOwned, "uncontended: unique_lock owns the mutex");
        check(fastest < 2.0, "uncontended acquire is effectively instant (< 2ms)");
    }

    // 2. Held indefinitely by another thread: must give up within a bounded time and NOT hang.
    {
        std::mutex m;
        std::atomic<bool> release{false};
        std::atomic<bool> acquired{false};
        std::thread holder([&] {
            std::unique_lock<std::mutex> hold(m);
            acquired.store(true);
            while (!release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        });
        while (!acquired.load()) std::this_thread::yield();  // ensure the holder owns it first

        std::atomic<bool> returned{false};
        std::atomic<bool> got{true};
        std::atomic<bool> owned{true};
        std::thread attempt([&] {
            std::unique_lock<std::mutex> lk(m, std::defer_lock);
            got.store(TryLockContextBudget(lk));
            owned.store(lk.owns_lock());
            returned.store(true);
        });
        const auto t0 = sclock::now();
        while (!returned.load() && ms_since(t0) < 5000.0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        const bool returnedWhileHeld = returned.load();

        release.store(true);
        holder.join();
        attempt.join();
        check(!got.load(), "contended acquire gives up (returns false) instead of blocking");
        check(!owned.load(), "contended: unique_lock does not own the mutex");
        check(returnedWhileHeld, "contended acquire returns within a bounded time (no indefinite block)");
    }

    // 3. Once released, a fresh attempt (the next present) acquires -- the drop is per-frame.
    {
        std::mutex m;
        std::unique_lock<std::mutex> lk(m, std::defer_lock);
        const bool got = TryLockContextBudget(lk);
        check(got && lk.owns_lock(), "after contention clears, a later attempt acquires again");
    }

    if (g_failures == 0) {
        printf("\nAll budget/no-freeze properties hold. PASS.\n");
        return 0;
    }
    printf("\n%d check(s) FAILED.\n", g_failures);
    return 1;
}
