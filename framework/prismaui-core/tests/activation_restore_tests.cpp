#include "PrismaUI/InputRegionPolicy.h"
#include <atomic>
#include <cassert>
#include <functional>
#include <mutex>
#include <utility>
namespace InputRegionPolicy = PrismaUI::InputRegionPolicy;
using ViewId = uint64_t;

struct Backend {
    bool IsInspectorVisible(ViewId) { return false; }
};
Backend g_backendInstance;

struct State {
    std::atomic<ViewId> focusedView{0};
    std::atomic<ViewId> activationRestoreView{0};
    std::atomic<uint32_t> activationRestoreCursorPolicy{0};
    std::atomic<ViewId> localInspectorOwner{0};
    std::atomic<bool> focusPauseGame{true};
    std::atomic<bool> focusDisableFocusMenu{true};
    std::mutex inputRegionMutex;
    InputRegionPolicy::State inputRegionState;
    InputRegionPolicy::State activationRestoreInputState;
    Backend* backend = &g_backendInstance;
} st;

std::function<void()> queued;
bool accept = true, valid = true, visible = true, panel = false;
bool IsValid(ViewId) { return valid; }
bool IsRuntimeViewVisible(ViewId) { return visible; }
bool IsAnyPanelVisible(ViewId) { return panel; }
void EndInspectorGesture() {}
void SyncVanillaCursorVisibility() {}
std::uint32_t g_appliedCursorPolicy = 0;
ViewId g_appliedCursorView = 0;
int g_trackedCursorResets = 0;
namespace WebInput {
void SetCaptureActive(bool, bool) {}
bool IsCaptureActive() { return true; }
bool IsEscapeOwned() { return true; }
int GetTrackedCursorX() { return 0; }
int GetTrackedCursorY() { return 0; }
void ResetTrackedCursor() { ++g_trackedCursorResets; }
std::uint32_t GetViewCursorPolicy(ViewId) { return 1; }
void SetViewCursorPolicy(ViewId view, std::uint32_t policy) {
    g_appliedCursorView = view;
    g_appliedCursorPolicy = policy;
}
}
namespace VirtualPointer {
void CancelHeldClick() noexcept {}
void DiscardHeldClick() noexcept {}
void ResetSession() noexcept {}
}
namespace logger {
template<class... T> void info(T&&...) {}
template<class... T> void warn(T&&...) {}
}
namespace GameThreadDispatcher {
bool Dispatch(std::function<void()> task, ViewId) {
    if (!accept) return false;
    queued = std::move(task); return true;
}
}
bool FocusImpl(ViewId view, bool pause, bool menu, InputRegionPolicy::CaptureMode mode) {
    assert(pause && menu);
    st.focusedView = view;
    InputRegionPolicy::SetFocusedView(st.inputRegionState, view);
    InputRegionPolicy::SetMode(st.inputRegionState, mode);
    return true;
}
bool SetInputRegions(ViewId view, const InputRegionPolicy::InputRegion* regions, uint32_t count) {
    InputRegionPolicy::SetRegions(st.inputRegionState, view, {regions, count}); return true;
}
#include "activation_under_test.inc"
int main() {
    for (bool dispatcher : {true, false}) {
        accept = dispatcher;
        for (auto mode : {InputRegionPolicy::CaptureMode::Full, InputRegionPolicy::CaptureMode::OverlayRegions}) {
            FocusImpl(7, true, true, mode);
            InputRegionPolicy::InputRegion region{0, 0, 100, 100};
            SetInputRegions(7, &region, 1);
            OnWindowActivation(false);
            assert(st.activationRestoreCursorPolicy.load() == 1);
            st.focusedView = 0;
            InputRegionPolicy::ClearFocusedView(st.inputRegionState, 7);
            OnWindowActivation(true);
            if (!dispatcher) {
                assert(!queued);
                assert(st.focusedView == 0);
                continue;
            }
            assert(queued && st.focusedView == 0);
            const int resetsBefore = g_trackedCursorResets;
            auto callback = std::move(queued); callback();
            assert(st.focusedView == 7);
            assert(g_appliedCursorView == 7 && g_appliedCursorPolicy == 1);
            assert(g_trackedCursorResets == resetsBefore + 1);
            assert(InputRegionPolicy::ShouldSwallowMouse(st.inputRegionState, 50, 50));
            assert(InputRegionPolicy::ShouldSwallowMouse(st.inputRegionState, 150, 150) ==
                   (mode == InputRegionPolicy::CaptureMode::Full));
        }
    }
    accept = true;
    FocusImpl(7, true, true, InputRegionPolicy::CaptureMode::Full);
    OnWindowActivation(false);
    st.focusedView = 0;
    InputRegionPolicy::ClearFocusedView(st.inputRegionState, 7);
    OnWindowActivation(true);
    assert(queued && st.focusedView == 0);
    visible = false;
    auto callback = std::move(queued); callback();
    assert(st.focusedView == 0);
}
