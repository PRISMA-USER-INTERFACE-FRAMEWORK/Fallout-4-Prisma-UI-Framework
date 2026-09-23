#include "WebRuntime.h"
#include "WebRuntimeInternal.h"
#ifndef PRISMAUI_FO4VR
#include "VirtualPointer.h"
#endif
#include "GameThreadDispatcher.h"
#include "InputOwnershipPolicy.h"
#include "InputRouting.h"
#include "WebInput.h"
namespace PrismaUI::WebRuntime {
    namespace {
        auto& st = Internal::RT();
        using namespace Internal;
        constexpr uint32_t kPrismaPointerCaptureModifier = 1u << 30;
        constexpr uint32_t kPrismaPointerCaptureContinuesModifier = 1u << 31;
        constexpr uint32_t kCtrlModifier = 1u << 2;
        constexpr int kInspectorTitleHeight = 24;
        constexpr int kInspectorResizeGrip = 14;
        constexpr int kInspectorMinimumSize = 32;
    }
    void QueueActivationRestore(ViewId view, bool pauseGame, bool disableFocusMenu,
                                InputRegionPolicy::State inputState) {
        auto restore = [view, pauseGame, disableFocusMenu, inputState = std::move(inputState)] {
            if (st.focusedView.load(std::memory_order_acquire) != 0 || !IsValid(view) ||
                !IsRuntimeViewVisible(view) || IsAnyPanelVisible(view))
                return;
            const bool restored = FocusImpl(view, pauseGame, disableFocusMenu, inputState.mode);
            if (restored) SetInputRegions(view, inputState.regions.data(),
                                          static_cast<uint32_t>(inputState.regions.size()));
            logger::info("[WebInput] activation focus restore {} (view={})", restored ? "succeeded" : "failed", view);
        };
#ifndef PRISMAUI_FO4VR
        if (GameThreadDispatcher::Dispatch(restore, view)) return;
#endif
        logger::warn("[WebInput] activation focus restore dropped (verified dispatcher rejected it, view={})", view);
    }
    bool OnWindowActivation(bool active) {
        if (!st.backend) return false;
        const ViewId owner = st.localInspectorOwner.load(std::memory_order_acquire);
        if (owner && st.backend->IsInspectorVisible(owner)) {
            EndInspectorGesture();
#ifndef PRISMAUI_FO4VR
            VirtualPointer::CancelHeldClick();
#endif
            WebInput::SetCaptureActive(active, active);
            SyncVanillaCursorVisibility();
            logger::info("[WebInput] inspector capture {} with window activation", active ? "resumed" : "suspended");
            return true;
        }
        if (!active) {
#ifndef PRISMAUI_FO4VR
            VirtualPointer::CancelHeldClick();
#endif
            const ViewId focused = st.focusedView.load(std::memory_order_acquire);
            if (focused) {
                std::lock_guard lock(st.inputRegionMutex);
                st.activationRestoreInputState = st.inputRegionState;
                st.activationRestoreView.store(focused, std::memory_order_release);
            }
            return false;
        }
        const ViewId restore = st.activationRestoreView.exchange(0, std::memory_order_acq_rel);
        if (!restore) return false;
        InputRegionPolicy::State inputState;
        {
            std::lock_guard lock(st.inputRegionMutex);
            inputState = std::move(st.activationRestoreInputState);
        }
        QueueActivationRestore(restore, st.focusPauseGame.load(std::memory_order_acquire),
                               st.focusDisableFocusMenu.load(std::memory_order_acquire), std::move(inputState));
        return false;
    }
    bool HasAnyActiveFocus() { return st.focusedView.load() != 0; }

    void MapClientPointToBrowser(int& x, int& y) { ClientToBackbuffer(x, y); }
    bool SetInputRegions(ViewId view, const InputRegionPolicy::InputRegion* regions, uint32_t count) {
        if (!view || !IsValid(view)) return false;
        if (st.focusedView.load() != view) return false;
        if (count > 0 && !regions) return false;
        std::lock_guard lock(st.inputRegionMutex);
        InputRegionPolicy::SetRegions(st.inputRegionState, view, {regions, count});
        return true;
    }
    bool ShouldRouteFocusedMouse(int x, int y) {
        if (st.inspectorPointerCaptureTarget.load(std::memory_order_acquire) != 0) return true;
        if (st.backend) {
            int localX = 0, localY = 0;
            if (st.backend->InspectorTargetAt(x, y, localX, localY)) return true;
        }
        std::lock_guard lock(st.inputRegionMutex);
        return InputRegionPolicy::ShouldRouteMouse(st.inputRegionState, x, y);
    }
    bool IsDockCursorActive() { return false; }
    void ShowBootAnimation() {
        const auto view = st.bootView.load(std::memory_order_acquire);
        if (!view || !st.backend || !st.active.load(std::memory_order_acquire)) return;
        Show(view);
        SetOrder(view, -100000);
    }
    void SendMouseMove(int x, int y, uint32_t modifiers, bool leave, bool rescale) {
        if (rescale) ClientToBackbuffer(x, y);
        if (!st.backend) return;
        const bool pointerCaptured = (modifiers & kPrismaPointerCaptureModifier) != 0;
        if (!pointerCaptured) st.inspectorPointerCaptureTarget.store(0, std::memory_order_release);
        int localX = 0, localY = 0;
        const ViewId capturedInspector = UpdateInspectorGesture(x, y, localX, localY);
        if (capturedInspector) {
            if (pointerCaptured) {
                st.inspectorPointerCaptureTarget.store(capturedInspector, std::memory_order_release);
                st.inspectorPointerOriginX.store(x - localX, std::memory_order_release);
                st.inspectorPointerOriginY.store(y - localY, std::memory_order_release);
            }
            st.inspectorMouseTarget.store(capturedInspector, std::memory_order_release);
            InputRouting::DispatchMouse(*st.backend, capturedInspector,
                                         {localX, localY, PrismaUI::Web::MouseButton::None,
                                          PrismaUI::Web::MouseAction::Move, leave});
            return;
        }
        const ViewId pointerCapture = st.inspectorPointerCaptureTarget.load(std::memory_order_acquire);
        if (pointerCaptured && pointerCapture) {
            if (st.backend->VisibleInspectorView() == pointerCapture) {
                localX = x - st.inspectorPointerOriginX.load(std::memory_order_acquire);
                localY = y - st.inspectorPointerOriginY.load(std::memory_order_acquire);
                st.inspectorMouseTarget.store(pointerCapture, std::memory_order_release);
                InputRouting::DispatchMouse(*st.backend, pointerCapture,
                                             {localX, localY, PrismaUI::Web::MouseButton::None,
                                              PrismaUI::Web::MouseAction::Move, leave});
            }
            return;
        }
        const ViewId inspector = st.backend->InspectorTargetAt(x, y, localX, localY);
        const ViewId previousInspector = st.inspectorMouseTarget.exchange(inspector, std::memory_order_acq_rel);
        if (inspector) {
            InputRouting::DispatchMouse(*st.backend, inspector,
                                         {localX, localY, PrismaUI::Web::MouseButton::None,
                                          PrismaUI::Web::MouseAction::Move, leave});
            return;
        }
        if (previousInspector)
            InputRouting::DispatchMouse(*st.backend, previousInspector,
                                         {0, 0, PrismaUI::Web::MouseButton::None,
                                          PrismaUI::Web::MouseAction::Move, true});
        const ViewId target = st.inputTargetView.load(std::memory_order_acquire);
        InputRouting::DispatchMouse(*st.backend, target,
                                     {x, y, PrismaUI::Web::MouseButton::None, PrismaUI::Web::MouseAction::Move,
                                      leave});
    }
    void SendMouseClick(int x, int y, int button, bool up, int clickCount, uint32_t modifiers, bool rescale) {
        if (rescale) ClientToBackbuffer(x, y);
        (void)clickCount;
        if (!st.backend) return;
        const auto mouseButton = button == 0   ? PrismaUI::Web::MouseButton::Left
                                 : button == 1 ? PrismaUI::Web::MouseButton::Middle
                                                : PrismaUI::Web::MouseButton::Right;
        int localX = 0, localY = 0;
        const ViewId gestureInspector = st.backend->InspectorGestureTargetAt(x, y, localX, localY);
        int hoverX = 0, hoverY = 0;
        const ViewId hoverInspector = st.backend->InspectorTargetAt(x, y, hoverX, hoverY);
        const bool nativePointerCapture = (modifiers & kPrismaPointerCaptureModifier) != 0;
        const bool captureContinues = (modifiers & kPrismaPointerCaptureContinuesModifier) != 0;
        ViewId pointerCapture = st.inspectorPointerCaptureTarget.load(std::memory_order_acquire);
        if (!nativePointerCapture && pointerCapture) {
            st.inspectorPointerCaptureTarget.store(0, std::memory_order_release);
            pointerCapture = 0;
        }
        ViewId inspector = gestureInspector ? gestureInspector : hoverInspector;
        if (pointerCapture) {
            const bool captureLive = gestureInspector == pointerCapture ||
                                     st.backend->VisibleInspectorView() == pointerCapture;
            if (!captureLive) {
                if (up && !captureContinues)
                    st.inspectorPointerCaptureTarget.store(0, std::memory_order_release);
                return;
            }
            inspector = pointerCapture;
            if (gestureInspector != pointerCapture) {
                localX = x - st.inspectorPointerOriginX.load(std::memory_order_acquire);
                localY = y - st.inspectorPointerOriginY.load(std::memory_order_acquire);
            }
        } else if (inspector) {
            localX = gestureInspector ? localX : hoverX;
            localY = gestureInspector ? localY : hoverY;
        }
        if (inspector && !up && button == 0 && !pointerCapture &&
            (modifiers & kCtrlModifier) == 0) {
            int probeX = 0, probeY = 0;
            const bool atRightEdge =
                st.backend->InspectorTargetAt(x + kInspectorResizeGrip, y, probeX, probeY) != inspector;
            const bool atBottomEdge =
                st.backend->InspectorTargetAt(x, y + kInspectorResizeGrip, probeX, probeY) != inspector;
            if (atRightEdge && atBottomEdge) {
                const int originX = x - localX;
                const int originY = y - localY;
                const bool minimumWidth = st.backend->InspectorTargetAt(
                                              originX + kInspectorMinimumSize - 1, originY, probeX, probeY) == inspector;
                const bool minimumHeight = st.backend->InspectorTargetAt(
                                               originX, originY + kInspectorMinimumSize - 1, probeX, probeY) == inspector;
                if (!minimumWidth || !minimumHeight) return;
                st.backend->BeginInspectorResize(inspector, localX, localY);
            } else if (localY < kInspectorTitleHeight) {
                st.backend->BeginInspectorMove(inspector, localX, localY);
            }
        }
        if (inspector) {
            if (!up && !pointerCapture) {
                st.inspectorPointerCaptureTarget.store(inspector, std::memory_order_release);
                st.inspectorPointerOriginX.store(x - localX, std::memory_order_release);
                st.inspectorPointerOriginY.store(y - localY, std::memory_order_release);
            }
            st.inspectorMouseTarget.store(inspector, std::memory_order_release);
            InputRouting::DispatchMouse(*st.backend, inspector,
                                         {localX, localY, mouseButton,
                                          up ? PrismaUI::Web::MouseAction::Up : PrismaUI::Web::MouseAction::Down,
                                          false});
            if (up && !captureContinues) {
                st.inspectorPointerCaptureTarget.store(0, std::memory_order_release);
                if (hoverInspector != inspector) {
                    InputRouting::DispatchMouse(*st.backend, inspector,
                                                 {localX, localY, PrismaUI::Web::MouseButton::None,
                                                  PrismaUI::Web::MouseAction::Move, true});
                }
                st.inspectorMouseTarget.store(hoverInspector, std::memory_order_release);
            }
            return;
        }
        st.inspectorMouseTarget.store(0, std::memory_order_release);
        const ViewId target = st.inputTargetView.load(std::memory_order_acquire);
        InputRouting::DispatchMouse(
            *st.backend, target,
            {x, y, mouseButton, up ? PrismaUI::Web::MouseAction::Up : PrismaUI::Web::MouseAction::Down, false});
    }
    void SendMouseWheel(int x, int y, int deltaX, int deltaY, uint32_t modifiers, bool rescale) {
        if (rescale) ClientToBackbuffer(x, y);
        if (!st.backend) return;
        const bool pointerCaptured = (modifiers & kPrismaPointerCaptureModifier) != 0;
        ViewId pointerCapture = st.inspectorPointerCaptureTarget.load(std::memory_order_acquire);
        if (!pointerCaptured && pointerCapture) {
            st.inspectorPointerCaptureTarget.store(0, std::memory_order_release);
            pointerCapture = 0;
        }
        if (pointerCaptured && pointerCapture) {
            if (st.backend->VisibleInspectorView() == pointerCapture)
                InputRouting::DispatchScroll(*st.backend, pointerCapture, {deltaX, deltaY});
            return;
        }
        int localX = 0, localY = 0;
        const ViewId inspector = st.backend->InspectorTargetAt(x, y, localX, localY);
        const ViewId target = inspector ? inspector : st.inputTargetView.load(std::memory_order_acquire);
        InputRouting::DispatchScroll(*st.backend, target, {deltaX, deltaY});
    }
    void SendKey(int keyType, int windowsKeyCode, int nativeKeyCode, uint32_t modifiers, uint16_t character,
                 bool systemKey) {
        (void)modifiers;
        if (!st.backend) return;
        const auto action = keyType == 2   ? PrismaUI::Web::KeyInput::Action::KeyUp
                            : keyType == 3 ? PrismaUI::Web::KeyInput::Action::Character
                                           : PrismaUI::Web::KeyInput::Action::RawKeyDown;
        const ViewId inspector = st.backend->VisibleInspectorView();
        const ViewId target = inspector ? inspector : st.inputTargetView.load(std::memory_order_acquire);
        const auto keyValue = action == PrismaUI::Web::KeyInput::Action::Character
                                  ? static_cast<uintptr_t>(character)
                                  : static_cast<uintptr_t>(windowsKeyCode);
        InputRouting::DispatchKey(*st.backend, target,
                                  {action, keyValue, nativeKeyCode, systemKey});
    }
    void ShowDock() {
        if (!st.backend || !st.active.load()) return;
        const auto focused = st.focusedView.load(std::memory_order_acquire);
        const auto inspectorOwner = st.localInspectorOwner.load(std::memory_order_acquire);
        const auto view = st.dockView.load(std::memory_order_acquire);
        if (!view || !IsValid(view)) {
            st.dockVisible.store(false, std::memory_order_release);
            if (focused == 0 && inspectorOwner == 0) {
                SetInputTargetView(0);
                WebInput::SetCaptureActive(false, false);
                SyncVanillaCursorVisibility();
            }
            return;
        }
        const auto boot = st.bootView.load(std::memory_order_acquire);
        if (boot) Hide(boot);
        Show(view);
        SetOrder(view, 100000);
        st.dockVisible.store(true, std::memory_order_release);
        if (focused == 0 && inspectorOwner == 0) {
            SetInputTargetView(view);
            WebInput::SetCaptureActive(true, false);
        }
    }
    void HideDock() {
        const auto view = st.dockView.load(std::memory_order_acquire);
        if (view) Hide(view);
        st.dockVisible.store(false);
        if (st.focusedView.load() == 0 && st.localInspectorOwner.load(std::memory_order_acquire) == 0) {
            SetInputTargetView(0);
#ifndef PRISMAUI_FO4VR
            VirtualPointer::CancelHeldClick();
#endif
            WebInput::SetCaptureActive(false, false);
            SyncVanillaCursorVisibility();
        }
    }
    bool SetNativeGamepad(ViewId view, bool enabled) {
        if (!st.backend || !st.active.load() || !HasFocus(view) || !IsValid(view) ||
            st.backend->VisibleInspectorView()) return false;
#ifndef PRISMAUI_FO4VR
        VirtualPointer::CancelHeldClick();
#endif
        return st.backend->SetNativeGamepad(view, enabled);
    }
    bool UsesNativeGamepad(ViewId view) {
        return st.backend && view && HasFocus(view) && st.backend->UsesNativeGamepad(view);
    }
    void SendGamepad(ViewId view, const Web::GamepadInput& input) {
        if (UsesNativeGamepad(view)) st.backend->SendGamepad(view, input);
    }
    std::uint64_t ControllerInputGeneration(ViewId view) {
        if (!st.backend || !st.active.load() || !HasFocus(view) || !IsValid(view) ||
            st.backend->VisibleInspectorView()) return 0;
        return st.backend->ControllerInputGeneration(view);
    }
    void SendControllerKey(ViewId view, std::uint64_t generation, std::uint32_t key,
                           bool pressed, bool fresh) {
        if (st.backend && generation && HasFocus(view))
            st.backend->SendControllerKey(view, generation, key, pressed, fresh);
    }
}
