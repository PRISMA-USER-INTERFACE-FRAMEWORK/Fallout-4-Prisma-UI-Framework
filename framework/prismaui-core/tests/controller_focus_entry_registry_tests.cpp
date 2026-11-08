#include "PrismaUI/ControllerActions.h"
#include "PrismaUI/GameThreadDispatcher.h"
#include "PrismaUI/WebRuntime.h"

#include <atomic>
#include <chrono>
#include <deque>
#include <iostream>
#include <map>
#include <thread>
#include <vector>

namespace PrismaUI::WebRuntime::Test {
namespace {
std::map<ViewId, ViewState> g_views;
bool g_anyFocus = false;
}
ViewState& View(ViewId view) { return g_views[view]; }
bool& AnyFocus() { return g_anyFocus; }
void Reset()
{
    g_views.clear();
    g_anyFocus = false;
}
}

namespace PrismaUI::GameThreadDispatcher {
namespace {
bool g_admit = true;
std::deque<std::function<void()>> g_queue;
}
namespace Test {
bool& Admit() { return g_admit; }
std::size_t Pending() { return g_queue.size(); }
void Drain()
{
    while (!g_queue.empty()) {
        auto task = std::move(g_queue.front());
        g_queue.pop_front();
        task();
    }
}
void Reset()
{
    g_admit = true;
    g_queue.clear();
}
}
bool Dispatch(std::function<void()> task, std::uint64_t)
{
    if (!g_admit) return false;
    g_queue.push_back(std::move(task));
    return true;
}
}

namespace {
namespace CA = PrismaUI::ControllerActions;
namespace Web = PrismaUI::WebRuntime;
namespace Queue = PrismaUI::GameThreadDispatcher;

constexpr std::uint32_t kBack = 0x10020u;
constexpr std::uint32_t kStart = 0x10010u;
constexpr std::uint32_t kDUp = 0x10001u;
constexpr std::uint32_t kLB = 0x10100u;
constexpr std::uint32_t kRB = 0x10200u;
constexpr std::uint32_t kB = 0x12000u;
constexpr std::uint32_t kX = 0x14000u;
constexpr std::uint32_t kY = 0x18000u;
constexpr std::uint32_t kLS = 0x10040u;
constexpr std::uint32_t kRS = 0x10080u;

int failures = 0;

void Check(bool value, const char* message)
{
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

struct Probe {
    std::atomic<int> calls{0};
};

void Record(void* userdata)
{
    static_cast<Probe*>(userdata)->calls.fetch_add(1);
}

CA::HandleResult Press(std::uint32_t code) { return CA::HandleFocusEntry(code, true, false); }
CA::HandleResult Hold(std::uint32_t code) { return CA::HandleFocusEntry(code, false, false); }
CA::HandleResult Release(std::uint32_t code) { return CA::HandleFocusEntry(code, false, true); }

void Tap(std::uint32_t code)
{
    (void)Press(code);
    (void)Release(code);
}

void Reset(std::initializer_list<std::uint64_t> views)
{
    for (const auto view : views) CA::Clear(view);
    Queue::Test::Reset();
    Web::Test::Reset();
}

void ButtonsCoexistOnOneView()
{
    constexpr std::uint64_t view = 101;
    Probe back, x, dup;
    Check(CA::BindFocusEntry(view, kBack, Record, &back), "Back focus entry binds");
    Check(CA::BindFocusEntry(view, kX, Record, &x), "X focus entry binds on the same view");
    Check(CA::BindFocusEntry(view, kDUp, Record, &dup), "DUp focus entry binds on the same view");

    for (const auto code : {kBack, kX, kDUp}) {
        const auto result = Press(code);
        Check(result.mappingActive && result.consumeGameEvent, "every bound button claims its fresh press");
        Check(Release(code).consumeGameEvent, "every bound button owns its release");
    }
    Queue::Test::Drain();
    Check(back.calls == 1 && x.calls == 1 && dup.calls == 1,
          "each coexisting registration runs its own callback exactly once");

    const auto unbound = Press(kY);
    Check(!unbound.mappingActive && !unbound.consumeGameEvent, "an unbound button stays Fallout's");
    Reset({view});
}

void RebindReplacesOnlyThatButton()
{
    constexpr std::uint64_t view = 102;
    Probe back, oldX, newX, y;
    Check(CA::BindFocusEntry(view, kBack, Record, &back), "Back binds");
    Check(CA::BindFocusEntry(view, kX, Record, &oldX), "X binds");
    Check(CA::BindFocusEntry(view, kY, Record, &y), "Y binds");

    Check(Press(kX).consumeGameEvent, "old X press is admitted");
    Check(Queue::Test::Pending() == 1, "old X callback is queued");
    Check(CA::BindFocusEntry(view, kX, Record, &newX), "X rebinds");
    Queue::Test::Drain();
    Check(oldX.calls == 0, "a replaced X registration never runs its queued callback");
    Check(newX.calls == 0, "the replacement does not inherit the stale queued press");
    (void)Release(kX);

    Tap(kX);
    Tap(kBack);
    Tap(kY);
    Queue::Test::Drain();
    Check(newX.calls == 1, "X routes to the replacement callback");
    Check(back.calls == 1, "rebinding X does not delete Back");
    Check(y.calls == 1, "rebinding X does not delete Y");
    Reset({view});
}

void UnbindRemovesOnlyThatButton()
{
    constexpr std::uint64_t view = 103;
    Probe back, x, y;
    Check(CA::BindFocusEntry(view, kBack, Record, &back), "Back binds");
    Check(CA::BindFocusEntry(view, kX, Record, &x), "X binds");
    Check(CA::BindFocusEntry(view, kY, Record, &y), "Y binds");

    Check(Press(kX).consumeGameEvent, "X press is admitted before unbind");
    Check(CA::UnbindFocusEntry(view, kX), "X unbinds");
    Check(!CA::UnbindFocusEntry(view, kX), "a second X unbind reports nothing bound");
    Check(!CA::UnbindFocusEntry(view, kStart), "unbinding a never-bound button fails");
    const auto staleRelease = Release(kX);
    Check(!staleRelease.consumeGameEvent, "an unbound button no longer owns its release");
    Queue::Test::Drain();
    Check(x.calls == 0, "an unbound registration never runs its queued callback");

    const auto afterX = Press(kX);
    Check(!afterX.mappingActive && !afterX.consumeGameEvent, "X is Fallout's again after unbind");
    Tap(kBack);
    Tap(kY);
    Queue::Test::Drain();
    Check(back.calls == 1, "unbinding X preserves Back");
    Check(y.calls == 1, "unbinding X preserves Y");
    Reset({view});
}

void WorkshopDockSet()
{
    constexpr std::uint64_t view = 104;
    const std::uint32_t codes[] = {kBack, kLS, kRS, kStart, kDUp, kLB, kRB, kB, kX, kY};
    std::vector<Probe> probes(std::size(codes));
    for (std::size_t i = 0; i < std::size(codes); ++i) {
        Check(CA::BindFocusEntry(view, codes[i], Record, &probes[i]), "dock button binds");
    }
    for (const auto code : codes) Tap(code);
    Queue::Test::Drain();
    for (const auto& probe : probes) {
        Check(probe.calls == 1, "all ten Workshop dock focus entries survive together");
    }
    Reset({view});
}

void SequenceOwnershipIsPerButton()
{
    constexpr std::uint64_t view = 105;
    Probe back, x;
    Check(CA::BindFocusEntry(view, kBack, Record, &back), "Back binds");
    Check(CA::BindFocusEntry(view, kX, Record, &x), "X binds");

    Check(Press(kBack).consumeGameEvent, "Back press is admitted");
    Check(Press(kBack).consumeGameEvent, "a duplicate Back press sample stays owned");
    Check(Hold(kBack).consumeGameEvent, "a held Back stays owned");
    Check(Press(kX).consumeGameEvent, "X is admitted while Back is held");
    Check(Release(kBack).consumeGameEvent, "Back release is owned");
    Check(!Release(kBack).consumeGameEvent, "a second Back release is not owned");
    Check(Release(kX).consumeGameEvent, "X release is still owned after Back released");
    Queue::Test::Drain();
    Check(back.calls == 1, "duplicate Back samples dispatch once");
    Check(x.calls == 1, "X dispatches once");

    Check(Press(kBack).consumeGameEvent && Press(kX).consumeGameEvent, "Back and X both owned");
    CA::OnFocusAccepted(view);
    Check(!Release(kBack).consumeGameEvent, "focus acceptance resets Back ownership");
    Check(!Release(kX).consumeGameEvent, "focus acceptance resets X ownership");
    Queue::Test::Drain();
    Reset({view});
}

void AdmissionFailureFailsOpen()
{
    constexpr std::uint64_t view = 106;
    Probe x;
    Check(CA::BindFocusEntry(view, kX, Record, &x), "X binds");
    Queue::Test::Admit() = false;
    const auto refused = Press(kX);
    Check(refused.mappingActive && !refused.consumeGameEvent, "a refused dispatch leaves the press to Fallout");
    Check(!Release(kX).consumeGameEvent, "a refused dispatch does not own the release");
    Queue::Test::Admit() = true;
    Check(Press(kX).consumeGameEvent, "the next fresh press is admitted");
    (void)Release(kX);
    Queue::Test::Drain();
    Check(x.calls == 1, "only the admitted press runs");
    Reset({view});
}

void ClearDropsEveryButtonAndStaleCallback()
{
    constexpr std::uint64_t view = 107;
    constexpr std::uint64_t other = 108;
    Probe back, x, otherX;
    Check(CA::BindFocusEntry(view, kBack, Record, &back), "Back binds");
    Check(CA::BindFocusEntry(view, kX, Record, &x), "X binds");
    Check(CA::BindFocusEntry(other, kY, Record, &otherX), "another view binds Y");

    Check(Press(kBack).consumeGameEvent && Press(kX).consumeGameEvent, "both presses are admitted");
    CA::Clear(view);
    Queue::Test::Drain();
    Check(back.calls == 0 && x.calls == 0, "view destruction cancels every queued callback of that view");
    Check(!Press(kBack).mappingActive && !Press(kX).mappingActive, "destroyed view has no focus entries");
    Check(!CA::UnbindFocusEntry(view, kBack), "nothing left to unbind after destroy");
    Tap(kY);
    Queue::Test::Drain();
    Check(otherX.calls == 1, "destroying one view leaves other views' entries");
    Reset({view, other});
}

void ArbitrationPicksTopVisibleDock()
{
    constexpr std::uint64_t low = 109;
    constexpr std::uint64_t high = 110;
    constexpr std::uint64_t overlay = 111;
    Probe lowX, highX, lowBack, overlayX;
    Web::Test::View(low).order = 1;
    Web::Test::View(high).order = 5;
    Web::Test::View(overlay).order = 9;
    Web::Test::View(overlay).role = 1u;
    Check(CA::BindFocusEntry(low, kX, Record, &lowX), "low X binds");
    Check(CA::BindFocusEntry(low, kBack, Record, &lowBack), "low Back binds");
    Check(CA::BindFocusEntry(high, kX, Record, &highX), "high X binds");
    Check(CA::BindFocusEntry(overlay, kX, Record, &overlayX), "overlay X binds");

    Tap(kX);
    Tap(kBack);
    Queue::Test::Drain();
    Check(highX.calls == 1 && lowX.calls == 0 && overlayX.calls == 0, "X goes to the top visible dock");
    Check(lowBack.calls == 1, "Back falls to the only dock that bound it");

    Web::Test::View(high).hidden = true;
    Tap(kX);
    Queue::Test::Drain();
    Check(lowX.calls == 1 && highX.calls == 1, "a hidden dock yields X to the next dock");

    Web::Test::AnyFocus() = true;
    const auto focused = Press(kX);
    Check(!focused.mappingActive && !focused.consumeGameEvent, "focus entry is inert while a view holds focus");
    Check(Queue::Test::Pending() == 0, "nothing is dispatched while a view holds focus");
    Reset({low, high, overlay});
}

std::atomic<bool> g_selfUnbindOk{false};
std::uint64_t g_selfView = 0;

void UnbindSelf(void* userdata)
{
    Record(userdata);
    g_selfUnbindOk = CA::UnbindFocusEntry(g_selfView, kX);
}

struct Blocking {
    std::atomic<bool> entered{false};
    std::atomic<bool> release{false};
    std::atomic<int> calls{0};
};

void BlockUntilReleased(void* userdata)
{
    auto* state = static_cast<Blocking*>(userdata);
    state->entered = true;
    while (!state->release) std::this_thread::yield();
    state->calls.fetch_add(1);
}

void CancellationWaitsForRunningCallback()
{
    constexpr std::uint64_t view = 112;
    Probe x, back;
    g_selfView = view;
    Check(CA::BindFocusEntry(view, kX, UnbindSelf, &x), "self-unbinding X binds");
    Check(CA::BindFocusEntry(view, kBack, Record, &back), "Back binds");
    Tap(kX);
    Queue::Test::Drain();
    Check(x.calls == 1 && g_selfUnbindOk, "a callback may unbind its own button without deadlock");
    Tap(kBack);
    Queue::Test::Drain();
    Check(back.calls == 1, "a callback unbinding X leaves Back bound");

    Blocking blocking;
    Probe y;
    Check(CA::BindFocusEntry(view, kY, BlockUntilReleased, &blocking), "blocking Y binds");
    Tap(kY);
    std::thread gameThread([] { Queue::Test::Drain(); });
    while (!blocking.entered) std::this_thread::yield();
    std::atomic<bool> rebound{false};
    std::thread binder([&] {
        rebound = CA::BindFocusEntry(view, kY, Record, &y);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    Check(!rebound, "rebinding Y waits for its running callback on another thread");
    blocking.release = true;
    gameThread.join();
    binder.join();
    Check(rebound && blocking.calls == 1, "rebinding Y completes after the running callback returns");
    Tap(kBack);
    Queue::Test::Drain();
    Check(back.calls == 2, "waiting on Y never touched Back");
    Reset({view});
}
}

int main()
{
    ButtonsCoexistOnOneView();
    RebindReplacesOnlyThatButton();
    UnbindRemovesOnlyThatButton();
    WorkshopDockSet();
    SequenceOwnershipIsPerButton();
    AdmissionFailureFailsOpen();
    ClearDropsEveryButtonAndStaleCallback();
    ArbitrationPicksTopVisibleDock();
    CancellationWaitsForRunningCallback();

    if (failures) {
        std::cerr << failures << " focus-entry registry check(s) failed\n";
        return 1;
    }
    std::cout << "controller focus-entry registry tests passed\n";
    return 0;
}
