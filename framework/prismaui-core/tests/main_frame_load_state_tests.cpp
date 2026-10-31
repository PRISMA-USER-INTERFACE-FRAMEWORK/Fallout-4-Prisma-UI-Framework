#include "Engine/MainFrameLoadState.h"
#include "Engine/ViewLoadFailureGate.h"

#include <iostream>

namespace {

int failures = 0;

void Check(bool value, const char* message) {
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

}  // namespace

int main() {
    using PrismaUI::Engine::MainFrameLoadState;
    using PrismaUI::Engine::ViewLoadFailureGate;

    MainFrameLoadState state;
    Check(!state.IsLoaded(), "a new browser is not loaded");

    state.OnLoadStart(true);
    Check(!state.IsLoaded(), "the first main-frame navigation is not ready before completion");
    state.OnLoadEnd(true, true);
    Check(state.IsLoaded(), "a successful main-frame load becomes ready");

    // Preserve the prior Host contract: a normal reload of an already healthy shell does not make
    // queued framework commands stop flowing merely because OnLoadStart fired.
    state.OnLoadStart(true);
    Check(state.IsLoaded(), "a healthy main-frame reload preserves existing readiness");
    state.OnLoadEnd(true, true);
    Check(state.IsLoaded(), "a healthy reload remains ready after completion");

    // HTTP failure is terminal but not successful. Completion must not promote the failed page.
    state.OnLoadStart(true);
    state.OnLoadError(true);
    state.OnLoadEnd(true, false);
    Check(!state.IsLoaded(), "an HTTP failure stays failed after terminal OnLoadEnd");

    state.OnLoadStart(true);
    Check(!state.IsLoaded(), "recovery from a failure waits for the new navigation to complete");
    state.OnLoadEnd(true, true);
    Check(state.IsLoaded(), "a later successful navigation recovers from an earlier failure");

    // CEF may deliver a status-0 terminal OnLoadEnd for the failed request after the replacement
    // navigation has already started. That stale failed completion must not finish the recovery.
    state.OnLoadStart(true);
    state.OnLoadError(true);
    Check(!state.IsLoaded(), "a main-frame network error is not ready");
    state.OnLoadStart(true);
    Check(!state.IsLoaded(), "network-error recovery waits for completion");
    state.OnLoadEnd(true, false);
    Check(!state.IsLoaded(),
          "the failed request's delayed status-0 completion cannot finish its retry");
    state.OnLoadEnd(true, true);
    Check(state.IsLoaded(), "the replacement navigation's successful completion restores readiness");

    MainFrameLoadState zeroStatus;
    zeroStatus.OnLoadStart(true);
    zeroStatus.OnLoadEnd(true, false);
    Check(!zeroStatus.IsLoaded(), "status zero is never treated as an initial successful document");

    state.OnLoadStart(false);
    state.OnLoadError(false);
    state.OnLoadEnd(false, true);
    Check(state.IsLoaded(), "a subframe failure does not poison a healthy main frame");

    // Shell-hosted views are child frames inside one HostClient. Their iframe load event can still
    // make the shell emit `domReady` after HTTP 404. The per-view gate must remain closed through
    // Failed -> Recovering and open only after that exact owned root reports a successful result.
    ViewLoadFailureGate gate;
    constexpr std::uint64_t kViewA = 41;
    constexpr std::uint64_t kViewB = 42;

    Check(ViewLoadFailureGate::ParseViewId("41") == kViewA,
          "domReady payload parsing accepts an exact non-zero view id");
    Check(ViewLoadFailureGate::ParseViewId("41x") == 0,
          "domReady payload parsing rejects trailing junk");
    Check(ViewLoadFailureGate::ParseViewId("0") == 0,
          "domReady payload parsing rejects view zero");

    Check(gate.AllowDomReady(kViewA), "a new shell-hosted view may report DOM ready");
    gate.OnLoadStart(kViewA);
    Check(gate.AllowDomReady(kViewA),
          "a healthy view's ordinary navigation does not create a failure gate");

    gate.OnLoadError(kViewA);
    Check(!gate.AllowDomReady(kViewA),
          "a failed shell-hosted view cannot overwrite kLoadFailed with DOM ready");
    Check(gate.AllowDomReady(kViewB),
          "one failed shell-hosted view does not block another view");

    gate.OnLoadSuccess(kViewA);
    Check(!gate.AllowDomReady(kViewA),
          "a completion delivered after an error cannot clear a failed view");

    gate.OnLoadStart(kViewA);
    Check(!gate.AllowDomReady(kViewA),
          "a recovering view stays blocked while its replacement root is loading");
    gate.OnLoadStart(kViewA);
    Check(!gate.AllowDomReady(kViewA),
          "repeated root starts cannot reopen a recovering view");

    // The proxy intentionally does not call OnLoadSuccess for status 0 or 400+. Only the successful
    // 200-399 terminal result for the replacement navigation reaches this transition.
    Check(!gate.AllowDomReady(kViewA),
          "the failed request's stale terminal result leaves recovery blocked");
    gate.OnLoadSuccess(kViewA);
    Check(gate.AllowDomReady(kViewA),
          "only successful owned-root recovery completion reopens DOM ready");

    gate.OnLoadError(kViewA);
    gate.OnLoadStart(kViewA);
    gate.OnLoadError(kViewA);
    gate.OnLoadSuccess(kViewA);
    Check(!gate.AllowDomReady(kViewA),
          "a recovery that fails again stays blocked even if CEF follows with OnLoadEnd");
    gate.OnLoadStart(kViewA);
    gate.OnLoadSuccess(kViewA);
    Check(gate.AllowDomReady(kViewA),
          "a later successful retry recovers after repeated failure");

    gate.OnLoadError(0);
    gate.OnLoadStart(0);
    gate.OnLoadSuccess(0);
    Check(gate.AllowDomReady(0), "unattributed shell traffic is never stored as a failed view");

    if (failures != 0) {
        std::cerr << failures << " load-state test(s) failed\n";
        return 1;
    }

    std::cout << "view load state tests passed\n";
    return 0;
}
