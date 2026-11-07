#include "PrismaUI/InputOwnershipPolicy.h"
#include "PrismaUI/UltralightDevToolsPolicy.h"

#include <cstdio>

using PrismaUI::InputOwnershipPolicy::AfterFocusedViewRelease;
using PrismaUI::InputOwnershipPolicy::CaptureOwner;
using PrismaUI::InputOwnershipPolicy::ShouldRouteMouse;
using PrismaUI::UltralightDevToolsPolicy::NormalizePort;

static int failures = 0;

static void expect(bool condition, const char* message) {
    std::printf("[%s] %s\n", condition ? "PASS" : "FAIL", message);
    if (!condition) ++failures;
}

struct LocalInspectorHarness {
    static constexpr unsigned focusOwner = 1u << 0;
    static constexpr unsigned inspectorOwner = 1u << 1;

    bool enabled = true;
    bool ownerVisible = true;
    bool creationSucceeds = true;
    bool maskApplicationSucceeds = true;
    bool inspectorVisible = false;
    bool capture = false;
    bool swallow = false;
    bool dockVisible = false;
    bool focused = false;
    bool requestInFlight = false;
    bool dispatchSucceeds = true;
    bool teardownPending = false;
    unsigned inputOwners = 0;

    bool acquireInput(unsigned owner) {
        if (inputOwners & owner) return true;
        if (inputOwners == 0 && !maskApplicationSucceeds) return false;
        inputOwners |= owner;
        return true;
    }

    bool releaseInput(unsigned owner) {
        if (!(inputOwners & owner)) return true;
        if ((inputOwners & ~owner) != 0) {
            inputOwners &= ~owner;
            return true;
        }
        inputOwners = 0;
        return true;
    }

    bool playerControlsEnabled() const { return inputOwners == 0; }

    void restoreCaptureAfterFocusRelease() {
        if (inputOwners & inspectorOwner) {
            capture = true;
            swallow = true;
            return;
        }
        const auto owner = AfterFocusedViewRelease(dockVisible);
        capture = owner != CaptureOwner::kNone;
        swallow = false;
    }

    bool open() {
        if (!enabled || !ownerVisible || inspectorVisible || teardownPending || !creationSucceeds) return false;
        requestInFlight = true;
        if (!dispatchSucceeds) {
            requestInFlight = false;
            return false;
        }
        backendStateChanged(true);
        return inspectorVisible;
    }

    bool toggle() {
        if (requestInFlight) return false;
        if (inspectorVisible) {
            close();
            return true;
        }
        return open();
    }

    void backendStateChanged(bool visible) {
        requestInFlight = false;
        if (visible) {
            if (!acquireInput(inspectorOwner)) {
                inspectorVisible = false;
                return;
            }
            inspectorVisible = true;
            capture = true;
            swallow = true;
            return;
        }
        teardownPending = false;
        inspectorVisible = false;
        releaseInput(inspectorOwner);
        const auto owner = focused ? CaptureOwner::kFocusedView : AfterFocusedViewRelease(dockVisible);
        capture = owner != CaptureOwner::kNone;
        swallow = owner == CaptureOwner::kFocusedView;
    }

    bool focus() {
        focused = acquireInput(focusOwner);
        return focused;
    }

    bool unfocus() {
        focused = false;
        const bool released = releaseInput(focusOwner);
        restoreCaptureAfterFocusRelease();
        return released;
    }

    void showDock() {
        dockVisible = true;
        if (!focused && !(inputOwners & inspectorOwner)) {
            capture = true;
            swallow = false;
        }
    }

    void hideDock() {
        dockVisible = false;
        if (!focused && !(inputOwners & inspectorOwner)) {
            capture = false;
            swallow = false;
        }
    }

    void close() {
        if (!inspectorVisible && !requestInFlight && !teardownPending) return;
        teardownPending = true;
        if (!dispatchSucceeds) inspectorVisible = false;
    }

    void retryTeardown() {
        if (!teardownPending) return;
        teardownPending = false;
        backendStateChanged(false);
    }

    void destroyOwner() {
        ownerVisible = false;
        backendStateChanged(false);
    }

    void destroyOwnerOffWindowThread() {
        ownerVisible = false;
        requestInFlight = true;
    }

    void drainQueuedDestroy() {
        backendStateChanged(false);
    }
};

int main() {
    expect(NormalizePort(0) == 9222, "zero port falls back to the compatibility default");
    expect(NormalizePort(-1) == 9222, "negative port falls back to the compatibility default");
    expect(NormalizePort(65536) == 9222, "overflow port falls back to the compatibility default");
    expect(NormalizePort(65535) == 65535, "highest valid port is preserved");

    LocalInspectorHarness disabled;
    disabled.enabled = false;
    expect(!disabled.open() && !disabled.capture && disabled.playerControlsEnabled(),
           "disabled DevTools does not acquire capture");

    LocalInspectorHarness failed;
    failed.creationSucceeds = false;
    expect(!failed.open() && !failed.capture && failed.playerControlsEnabled(),
           "creation failure does not acquire capture");

    LocalInspectorHarness maskFailed;
    maskFailed.maskApplicationSucceeds = false;
    expect(!maskFailed.open() && maskFailed.playerControlsEnabled(),
           "mask application failure does not claim inspector ownership");

    LocalInspectorHarness dispatchOpenFailed;
    dispatchOpenFailed.dispatchSucceeds = false;
    expect(!dispatchOpenFailed.open() && !dispatchOpenFailed.inspectorVisible &&
               !dispatchOpenFailed.teardownPending && dispatchOpenFailed.playerControlsEnabled(),
           "rejected inspector open clears pending state without claiming input");

    LocalInspectorHarness local;
    expect(local.open() && local.inspectorVisible && local.capture && local.swallow &&
               !local.playerControlsEnabled(),
           "successful local creation owns input");
    expect(!local.open(), "a second open does not create a duplicate inspector");
    expect(local.toggle() && local.inspectorVisible && local.teardownPending &&
               !local.playerControlsEnabled(),
           "close request keeps inspector input masked until backend confirmation");
    local.backendStateChanged(false);
    expect(!local.inspectorVisible && !local.teardownPending && !local.capture && local.playerControlsEnabled(),
           "backend-confirmed close clears inspector state and restores player controls");
    local.close();
    expect(!local.inspectorVisible && !local.capture && local.playerControlsEnabled(),
           "close after confirmation is idempotent");

    LocalInspectorHarness focused;
    expect(focused.focus() && focused.open(), "focused view can open the local inspector");
    focused.close();
    expect(focused.inspectorVisible && focused.teardownPending && focused.capture && focused.swallow &&
               !focused.playerControlsEnabled(),
           "inspector close request retains capture until backend confirmation");
    focused.backendStateChanged(false);
    expect(focused.capture && focused.swallow && !focused.playerControlsEnabled(),
           "backend-confirmed inspector close retains the focused-view input mask");
    expect(focused.unfocus() && focused.playerControlsEnabled() && !focused.capture,
           "releasing the focused view finally restores player controls and capture");

    LocalInspectorHarness focusOnly;
    expect(focusOnly.focus() && !focusOnly.playerControlsEnabled(),
           "focused view alone owns the input mask");
    expect(focusOnly.unfocus() && focusOnly.playerControlsEnabled() && !focusOnly.capture,
           "focused-view-only release restores player controls and capture");

    LocalInspectorHarness duplicate;
    expect(duplicate.focus() && duplicate.focus() && duplicate.open() && !duplicate.open(),
           "duplicate owner acquisitions are idempotent");
    expect(duplicate.unfocus() && duplicate.capture && duplicate.swallow && !duplicate.playerControlsEnabled(),
           "releasing focus preserves active inspector capture and ownership");
    duplicate.close();
    expect(duplicate.teardownPending && duplicate.capture && duplicate.swallow && !duplicate.playerControlsEnabled(),
           "final inspector close keeps capture and player controls masked until confirmation");
    duplicate.backendStateChanged(false);
    expect(duplicate.playerControlsEnabled() && !duplicate.capture,
           "releasing the final owner restores player controls and capture");

    LocalInspectorHarness dock;
    dock.dockVisible = true;
    expect(dock.open(), "Dock-visible state can open the local inspector");
    dock.close();
    expect(dock.inspectorVisible && dock.teardownPending && !dock.playerControlsEnabled(),
           "Dock inspector close remains masked until confirmation");
    dock.backendStateChanged(false);
    expect(dock.capture && !dock.swallow, "inspector close restores passive Dock capture");

    LocalInspectorHarness dockTransitions;
    expect(dockTransitions.open() && dockTransitions.capture && dockTransitions.swallow,
           "inspector owns capture before Dock visibility transitions");
    dockTransitions.showDock();
    expect(dockTransitions.dockVisible && dockTransitions.capture && dockTransitions.swallow,
           "showing Dock does not replace active inspector capture");
    dockTransitions.hideDock();
    expect(!dockTransitions.dockVisible && dockTransitions.capture && dockTransitions.swallow,
           "hiding Dock does not disable active inspector capture");
    dockTransitions.close();
    expect(dockTransitions.capture && dockTransitions.swallow,
           "inspector close request still owns capture after Dock transitions");
    dockTransitions.backendStateChanged(false);
    expect(!dockTransitions.capture && dockTransitions.playerControlsEnabled(),
           "backend-confirmed inspector close releases capture after hidden Dock transition");

    LocalInspectorHarness destroyed;
    expect(destroyed.open(), "owner can open the local inspector before destruction");
    destroyed.destroyOwner();
    expect(!destroyed.inspectorVisible && !destroyed.capture, "owner destruction clears inspector capture");

    LocalInspectorHarness asyncDestroyed;
    expect(asyncDestroyed.open(), "owner can open before off-thread destruction");
    asyncDestroyed.destroyOwnerOffWindowThread();
    expect(!asyncDestroyed.playerControlsEnabled(),
           "off-thread destruction keeps inspector ownership until queued teardown");
    asyncDestroyed.drainQueuedDestroy();
    expect(asyncDestroyed.playerControlsEnabled(), "queued destruction releases inspector ownership");

    LocalInspectorHarness dispatchCloseFailed;
    expect(dispatchCloseFailed.open(), "owner can open before rejected close callback dispatch");
    dispatchCloseFailed.dispatchSucceeds = false;
    dispatchCloseFailed.close();
    expect(dispatchCloseFailed.teardownPending && !dispatchCloseFailed.inspectorVisible &&
               !dispatchCloseFailed.playerControlsEnabled(),
           "rejected inspector close callback keeps teardown pending and input masked after the backend hides");
    expect(!dispatchCloseFailed.open(), "pending teardown blocks inspector reopen");
    dispatchCloseFailed.dispatchSucceeds = true;
    dispatchCloseFailed.retryTeardown();
    expect(!dispatchCloseFailed.teardownPending && dispatchCloseFailed.playerControlsEnabled(),
           "safe teardown retry releases inspector ownership");

    expect(ShouldRouteMouse(CaptureOwner::kPassiveDock, false), "passive Dock remains a mouse route");
    expect(!ShouldRouteMouse(CaptureOwner::kNone, true), "no capture owner routes no mouse input");

    if (failures == 0) {
        std::puts("All local Ultralight DevTools policy assertions passed.");
        return 0;
    }
    std::printf("%d local Ultralight DevTools assertion(s) failed.\n", failures);
    return 1;
}
