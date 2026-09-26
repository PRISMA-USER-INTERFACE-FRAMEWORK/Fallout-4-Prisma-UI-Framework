#include "PCH.h"
#include "GameThreadDispatcher.h"
#include "GameThreadDispatchQueue.h"
#include "FreezeDiagnostics.h"
#include "WebRuntime.h"

#include <atomic>
#include <chrono>
#include <exception>
#include <utility>

namespace PrismaUI::GameThreadDispatcher {
namespace {

constexpr auto kWakeBudget = std::chrono::milliseconds(2);

std::atomic<DWORD> g_pluginLoadThreadId{0};
std::atomic<DWORD> g_gameThreadId{0};
std::atomic<DWORD> g_drainThreadId{0};
std::atomic<HWND> g_window{nullptr};
std::atomic<bool> g_ready{false};
std::atomic<bool> g_failed{false};
GameThreadDispatchQueue::Queue g_tasks;

void FailClosed(const char* reason, DWORD expected, DWORD actual) noexcept
{
    g_ready.store(false, std::memory_order_release);
    g_failed.store(true, std::memory_order_release);
    g_window.store(nullptr, std::memory_order_release);
    g_tasks.failClosed();
    logger::critical("[GameThreadDispatcher] {} (expected thread={}, actual thread={}); window-thread callbacks disabled",
                     reason, expected, actual);
}

bool PostWake() noexcept
{
    const HWND hwnd = g_window.load(std::memory_order_acquire);
    const UINT message = MessageId();
    if (!hwnd || !message) {
        return false;
    }
    if (::PostMessageW(hwnd, message, 0, 0)) return true;

    logger::critical("[GameThreadDispatcher] dispatch rejected: wake-failure (PostMessageW GLE={}); "
                     "window-thread callbacks disabled until the window is re-verified",
                     ::GetLastError());
    FailClosed("could not post the dispatcher wake message",
               g_gameThreadId.load(std::memory_order_acquire), ::GetCurrentThreadId());
    return false;
}

bool NoteRejected(GameThreadDispatchQueue::Rejection reason) noexcept
{
    static std::atomic<int64_t> s_lastLogMs{0};
    static std::atomic<uint32_t> s_suppressed{0};
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    auto last = s_lastLogMs.load(std::memory_order_relaxed);
    if (nowMs - last < 1000 || !s_lastLogMs.compare_exchange_strong(last, nowMs, std::memory_order_relaxed)) {
        s_suppressed.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    logger::warn("[GameThreadDispatcher] dispatch rejected: {} ({} similar rejections suppressed)",
                 GameThreadDispatchQueue::RejectionName(reason),
                 s_suppressed.exchange(0, std::memory_order_relaxed));
    return false;
}

}

UINT MessageId() noexcept
{
    static const UINT id = ::RegisterWindowMessageW(L"PrismaUI_F4.GameThreadDispatch.v1");
    return id;
}

void CaptureCurrentThread() noexcept
{
    const DWORD current = ::GetCurrentThreadId();
    DWORD expected = 0;
    if (g_pluginLoadThreadId.compare_exchange_strong(expected, current, std::memory_order_acq_rel)) {
        logger::info("[GameThreadDispatcher] captured F4SE plugin-load thread candidate {}", current);
        return;
    }
    if (expected != current) {
        logger::warn("[GameThreadDispatcher] plugin-load thread candidate changed from {} to {}", expected,
                     current);
    }
}

namespace {

DWORD VerifyWindowThread(HWND hwnd) noexcept
{
    const DWORD currentThread = ::GetCurrentThreadId();
    if (!MessageId()) {
        logger::critical("[GameThreadDispatcher] RegisterWindowMessageW failed, GLE={}; window-thread callbacks disabled",
                         ::GetLastError());
        FailClosed("could not register the dispatcher wake message", currentThread, 0);
        return 0;
    }

    DWORD processId = 0;
    const DWORD ownerThread = ::GetWindowThreadProcessId(hwnd, &processId);
    if (!ownerThread || processId != ::GetCurrentProcessId() || ownerThread != currentThread) {
        FailClosed("dispatcher attach did not run on the Fallout window thread", ownerThread, currentThread);
        return 0;
    }
    return ownerThread;
}

bool Publish(HWND hwnd, DWORD ownerThread) noexcept
{
    const HWND already = g_window.load(std::memory_order_acquire);
    if (already == hwnd && IsReady()) {
        return true;
    }

    g_gameThreadId.store(ownerThread, std::memory_order_release);
    g_window.store(hwnd, std::memory_order_release);
    g_tasks.attach();
    g_ready.store(true, std::memory_order_release);
    logger::info("[GameThreadDispatcher] verified Fallout window thread {} on HWND {:p}",
                 ownerThread, static_cast<void*>(hwnd));
    return true;
}

}

bool AttachWindow(HWND hwnd) noexcept
{
    if (!hwnd || g_failed.load(std::memory_order_acquire)) return false;
    const DWORD ownerThread = VerifyWindowThread(hwnd);
    return ownerThread && Publish(hwnd, ownerThread);
}

bool RecoverWindow(HWND hwnd) noexcept
{
    if (!hwnd) return false;
    if (!g_failed.load(std::memory_order_acquire)) return AttachWindow(hwnd);
    const DWORD ownerThread = VerifyWindowThread(hwnd);
    if (!ownerThread) return false;
    g_tasks.recoverAfterFailure();
    g_failed.store(false, std::memory_order_release);
    logger::warn("[GameThreadDispatcher] recovering after fail-closed: Fallout window thread {} re-verified; "
                 "callbacks rejected before recovery stay dropped", ownerThread);
    return Publish(hwnd, ownerThread);
}

void DetachWindow(HWND hwnd) noexcept
{
    HWND expected = hwnd;
    if (g_window.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel)) {
        g_ready.store(false, std::memory_order_release);
        g_gameThreadId.store(0, std::memory_order_release);
        g_tasks.detach();
    }
}

bool IsReady() noexcept
{
    return g_ready.load(std::memory_order_acquire) && !g_failed.load(std::memory_order_acquire);
}

bool IsGameThread() noexcept
{
    if (!IsReady()) return false;
    return ::GetCurrentThreadId() == g_gameThreadId.load(std::memory_order_acquire);
}

bool Dispatch(std::function<void()> task, uint64_t view)
{
    if (!task) return false;
    if (g_failed.load(std::memory_order_acquire)) return NoteRejected(GameThreadDispatchQueue::Rejection::Failed);

    const auto queued = g_tasks.tryEnqueue(std::move(task), view);
    if (!queued.accepted) return NoteRejected(queued.reason);
    if (!queued.needsWake) return true;
    if (PostWake()) return true;
    return false;
}

bool DispatchSafety(std::function<void()> task)
{
    if (!task) return false;
    if (g_failed.load(std::memory_order_acquire)) return NoteRejected(GameThreadDispatchQueue::Rejection::Failed);
    const auto queued = g_tasks.tryEnqueue(std::move(task), 0, true);
    if (!queued.accepted) return NoteRejected(queued.reason);
    if (!queued.needsWake) return true;
    if (PostWake()) return true;
    return false;
}

void DropView(uint64_t view) noexcept
{
    g_tasks.dropView(view);
    const DWORD drainThread = g_drainThreadId.load(std::memory_order_acquire);
    if (drainThread != 0 && ::GetCurrentThreadId() == drainThread) return;
    FreezeDiagnostics::BeginDropViewWait(view);
    struct DropGuard {
        ~DropGuard() { FreezeDiagnostics::EndDropViewWait(); }
    } dropGuard;
    while (!g_tasks.waitUntilNotExecutingFor(view, std::chrono::seconds(5))) {
        logger::critical("[GameThreadDispatcher] Destroy(view={}) still waiting for an in-flight "
                         "window-thread callback; userdata is not safe to free yet",
                         view);
    }
}

bool HandleWindowMessage(HWND hwnd, UINT message) noexcept
{
    const UINT registeredMessage = MessageId();
    if (!registeredMessage || message != registeredMessage) return false;

    const DWORD expectedThread = g_gameThreadId.load(std::memory_order_acquire);
    const DWORD currentThread = ::GetCurrentThreadId();
    const HWND liveWindow = g_window.load(std::memory_order_acquire);
    if (!IsReady() || hwnd != liveWindow) {

        return true;
    }
    if (currentThread != expectedThread) {
        FailClosed("dispatch message arrived on an unverified thread", expectedThread, currentThread);
        return true;
    }

    thread_local bool t_insideDrain = false;
    if (t_insideDrain) return true;

    struct InsideDrain {
        InsideDrain() { t_insideDrain = true; }
        ~InsideDrain() { t_insideDrain = false; }
    } insideDrain;

    struct DrainThreadScope {
        explicit DrainThreadScope(DWORD threadId)
        {
            g_drainThreadId.store(threadId, std::memory_order_release);
        }
        ~DrainThreadScope() { g_drainThreadId.store(0, std::memory_order_release); }
        DrainThreadScope(const DrainThreadScope&) = delete;
        DrainThreadScope& operator=(const DrainThreadScope&) = delete;
    } drainThread{currentThread};

    {
        struct DrainSession {
            DrainSession() { g_tasks.beginDrain(); }
            ~DrainSession() { g_tasks.endDrain(); }
        } drainSession;
        const auto deadline = std::chrono::steady_clock::now() + kWakeBudget;
        std::size_t ran = 0;
        while (ran < GameThreadDispatchQueue::kMaxTasksPerWake) {
            if (ran > 0 && std::chrono::steady_clock::now() >= deadline) break;
            auto task = g_tasks.takeNext([](uint64_t view) {
                return view == 0 || WebRuntime::IsValid(view);
            });
            if (!task) break;
            ++ran;
            struct FinishExecuting {
                ~FinishExecuting() {
                    g_tasks.finishExecuting();
                    FreezeDiagnostics::MarkIdle(FreezeDiagnostics::Lane::GameDispatch);
                }
            } finish;
            FreezeDiagnostics::MarkStage(FreezeDiagnostics::Lane::GameDispatch,
                                         FreezeDiagnostics::Stage::GameThreadCallback, task->view);
            try {
                if (task->fn) task->fn();
            } catch (const std::exception& e) {
                logger::error("[GameThreadDispatcher] callback threw: {}", e.what());
            } catch (...) {
                logger::error("[GameThreadDispatcher] callback threw an unknown exception");
            }
        }
    }

    if (g_tasks.takeWakeRearm()) {
        (void)PostWake();
    }
    return true;
}

}
