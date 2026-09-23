#include "WebRuntime.h"
#include "WebRuntimeInternal.h"
#ifndef PRISMAUI_FO4VR
#include "VirtualPointer.h"
#endif
#include "DevToolsConfig.h"
#include "GameThreadDispatcher.h"
#include "InputMask.h"
#include "InputOwnershipPolicy.h"
#include "InputRouting.h"
#include "Menus/FocusMenu/FocusMenu.h"
#include "Menus/PauseHold/PauseHold.h"
#include "Utils/ModulePath.h"
#include "WebInput.h"
#include <cmath>
#include <limits>
namespace PrismaUI::WebRuntime {
    namespace {
        auto& st = Internal::RT();
        using namespace Internal;
        void ApplyInspectorBackendStateChanged(ViewId owner, bool visible, bool fallback) {
            if (!owner) return;
            if (visible) {
                if (!st.backend || !IsValid(owner) || !st.backend->IsInspectorVisible(owner)) return;
                const ViewId activeOwner = st.localInspectorOwner.load(std::memory_order_acquire);
                const ViewId pendingOwner = st.localInspectorPending.load(std::memory_order_acquire);
                const ViewId teardownOwner = st.inspectorTeardownPending.load(std::memory_order_acquire);
                if (teardownOwner || (activeOwner && activeOwner != owner) || (!activeOwner && pendingOwner != owner)) {
                    st.backend->SetInspectorVisibility(owner, false);
                    const ViewId intendedOwner = activeOwner ? activeOwner : pendingOwner;
                    if (!teardownOwner && intendedOwner && intendedOwner != owner) {
#ifndef PRISMAUI_FO4VR
                        VirtualPointer::CancelHeldClick();
#endif
                        st.backend->SetInspectorVisibility(intendedOwner, true);
                    }
                    return;
                }
                if (!InputMask::Acquire(InputMask::Owner::kInspector)) {
                    logger::critical("[PrismaUI] local inspector refused because player input could not be masked "
                                     "(owner={})", owner);
                    st.backend->SetInspectorVisibility(owner, false);
                    return;
                }
                st.localInspectorOwner.store(owner, std::memory_order_release);
                ViewId expectedPending = owner;
                st.localInspectorPending.compare_exchange_strong(expectedPending, 0, std::memory_order_acq_rel);
                st.inspectorMouseTarget.store(0, std::memory_order_release);
                const bool inspectorOnlySession =
                    st.focusedView.load(std::memory_order_acquire) == 0 &&
                    !st.focusSession.load(std::memory_order_acquire);
                if (inspectorOnlySession && !InputMask::Acquire(InputMask::Owner::kFocus)) {
                    logger::critical("[PrismaUI] local inspector refused because focus input could not be masked "
                                     "(owner={})", owner);
                    st.localInspectorOwner.store(0, std::memory_order_release);
                    (void)InputMask::Release(InputMask::Owner::kInspector);
                    st.backend->SetInspectorVisibility(owner, false);
                    return;
                }
                WebInput::SetCaptureActive(true, true);
                if (inspectorOnlySession) {
                    st.focusSession.store(true, std::memory_order_release);
                    if (auto* menuCursor = RE::MenuCursor::GetSingleton()) {
                        menuCursor->RegisterCursor();
                    }
                    PauseHold::Set(false);
                    FocusMenu::Open(false);
                }
                return;
            }
            if (!fallback &&
                st.localInspectorOwner.load(std::memory_order_acquire) == owner &&
                st.localInspectorPending.load(std::memory_order_acquire) == owner &&
                st.backend && st.backend->IsReady() && IsValid(owner)) {
                return;
            }
            if (fallback && st.backend && st.backend->IsInspectorVisible(owner)) {
                st.backend->SetInspectorVisibility(owner, false);
                st.inspectorTeardownRetryAt.store(HealthNowMs(), std::memory_order_release);
                return;
            }
            ViewId expectedOwner = owner;
            const bool releasedOwner = st.localInspectorOwner.compare_exchange_strong(
                expectedOwner, 0, std::memory_order_acq_rel);
            ViewId expectedPending = owner;
            const bool releasedPending = st.localInspectorPending.compare_exchange_strong(
                expectedPending, 0, std::memory_order_acq_rel);
            ViewId expectedTeardown = owner;
            const bool releasedTeardown = st.inspectorTeardownPending.compare_exchange_strong(
                expectedTeardown, 0, std::memory_order_acq_rel);
            if (!releasedOwner && !releasedPending && !releasedTeardown) return;
            if (!fallback) st.inspectorTeardownRetryAt.store(0, std::memory_order_release);
            if (!releasedOwner && !releasedPending) return;
            st.inspectorMouseTarget.store(0, std::memory_order_release);
            (void)InputMask::Release(InputMask::Owner::kInspector);
            if (st.focusedView.load(std::memory_order_acquire) == owner && !IsRuntimeViewVisible(owner))
                Unfocus(owner);
            const ViewId focused = st.focusedView.load(std::memory_order_acquire);
#ifndef PRISMAUI_FO4VR
            VirtualPointer::CancelHeldClick();
#endif
            if (st.backend) {
                if (owner != focused) st.backend->UnfocusView(owner);
                if (focused) st.backend->FocusView(focused);
            }
            if (focused) {
                SetInputTargetView(InputRouting::DestinationForFocusedView(focused));
                WebInput::SetCaptureActive(true, true);
                SyncVanillaCursorVisibility();
                return;
            }
            if (st.focusSession.exchange(false, std::memory_order_acq_rel)) {
                FocusMenu::Close();
                PauseHold::Set(false);
                (void)InputMask::Release(InputMask::Owner::kFocus);
                if (auto* menuCursor = RE::MenuCursor::GetSingleton()) {
                    menuCursor->UnregisterCursor();
                }
            }
            const auto captureOwner = InputOwnershipPolicy::AfterFocusedViewRelease(st.dockVisible.load());
            SetInputTargetView(captureOwner == InputOwnershipPolicy::CaptureOwner::kPassiveDock
                                   ? st.dockView.load(std::memory_order_acquire)
                                   : 0);
            WebInput::SetCaptureActive(captureOwner != InputOwnershipPolicy::CaptureOwner::kNone, false);
            SyncVanillaCursorVisibility();
        }
    }
    void RetryInspectorTeardown() {
        ViewId owner = st.inspectorTeardownPending.load(std::memory_order_acquire);
        if (!owner) {
            const ViewId activeOwner = st.localInspectorOwner.load(std::memory_order_acquire);
            const ViewId pendingOwner = st.localInspectorPending.load(std::memory_order_acquire);
            if (!activeOwner && pendingOwner &&
                (!st.backend || !st.backend->IsReady() || !IsValid(pendingOwner))) {
                OnInspectorBackendStateChanged(pendingOwner, false);
                return;
            }
            if (activeOwner && (!st.backend || !st.backend->IsReady() || !IsValid(activeOwner))) {
                OnInspectorBackendStateChanged(activeOwner, false);
                return;
            }
            if (!activeOwner) return;
            const int64_t now = HealthNowMs();
            const int64_t lastPresent = st.lastPresentMs.load(std::memory_order_acquire);
            const int64_t reference = lastPresent != 0 ? lastPresent : st.recoveryStartMs.load(std::memory_order_acquire);
            if (reference == 0 || now - reference <= 3000) return;
            if (const HWND hwnd = st.inputHwnd.load(std::memory_order_acquire); hwnd && IsIconic(hwnd)) return;
            SetInspectorVisibility(activeOwner, false);
            return;
        }
        const int64_t now = HealthNowMs();
        const int64_t retryAt = st.inspectorTeardownRetryAt.load(std::memory_order_acquire);
        if (retryAt != 0 && now - retryAt < 5000) return;
        int64_t expectedRetryAt = retryAt;
        if (!st.inspectorTeardownRetryAt.compare_exchange_strong(expectedRetryAt, now, std::memory_order_acq_rel))
            return;
#ifndef PRISMAUI_FO4VR
        if (GameThreadDispatcher::DispatchSafety([owner, now] {
                int64_t expected = now;
                st.inspectorTeardownRetryAt.compare_exchange_strong(expected, 0, std::memory_order_acq_rel);
                if (st.inspectorTeardownPending.load(std::memory_order_acquire) == owner)
                    ApplyInspectorBackendStateChanged(owner, false, true);
            }))
            return;
#endif
        st.inspectorTeardownRetryAt.store(0, std::memory_order_release);
        if (auto* task = F4SE::GetTaskInterface()) {
            const int64_t fallbackAt = HealthNowMs();
            st.inspectorTeardownRetryAt.store(fallbackAt, std::memory_order_release);
            task->AddTask([owner, fallbackAt] {
                int64_t expected = fallbackAt;
                st.inspectorTeardownRetryAt.compare_exchange_strong(expected, 0, std::memory_order_acq_rel);
                if (st.inspectorTeardownPending.load(std::memory_order_acquire) == owner)
                    ApplyInspectorBackendStateChanged(owner, false, true);
            });
            return;
        }
        logger::critical("[PrismaUI] inspector teardown remains pending because no safe dispatch path is available "
                         "(owner={})", owner);
    }
    void OnInspectorBackendStateChanged(ViewId owner, bool visible) {
        if (!owner) return;
#ifndef PRISMAUI_FO4VR
        if (!GameThreadDispatcher::IsGameThread()) {
            if (GameThreadDispatcher::DispatchSafety(
                    [owner, visible] { ApplyInspectorBackendStateChanged(owner, visible, false); }))
                return;
            const ViewId activeOwner = st.localInspectorOwner.load(std::memory_order_acquire);
            const ViewId pendingOwner = st.localInspectorPending.load(std::memory_order_acquire);
            const ViewId teardownOwner = st.inspectorTeardownPending.load(std::memory_order_acquire);
            if (visible) {
                if (teardownOwner || (activeOwner && activeOwner != owner) || (!activeOwner && pendingOwner != owner)) {
                    if (st.backend) {
                        st.backend->SetInspectorVisibility(owner, false);
                        const ViewId intendedOwner = activeOwner ? activeOwner : pendingOwner;
                        if (!teardownOwner && intendedOwner && intendedOwner != owner) {
#ifndef PRISMAUI_FO4VR
                            VirtualPointer::CancelHeldClick();
#endif
                            st.backend->SetInspectorVisibility(intendedOwner, true);
                        }
                    }
                    return;
                }
                if (activeOwner == owner) {
                    ViewId expectedPending = owner;
                    st.localInspectorPending.compare_exchange_strong(expectedPending, 0, std::memory_order_acq_rel);
                    return;
                }
                if (st.backend) st.backend->SetInspectorVisibility(owner, false);
                logger::critical("[PrismaUI] inspector open failed closed because the window-thread dispatcher "
                                 "is unavailable (owner={})", owner);
            } else {
                if (activeOwner != owner && pendingOwner != owner && teardownOwner != owner) return;
                ViewId expectedTeardown = 0;
                if (!st.inspectorTeardownPending.compare_exchange_strong(expectedTeardown, owner,
                                                                         std::memory_order_acq_rel) &&
                    expectedTeardown != owner)
                    return;
                logger::critical("[PrismaUI] inspector close queued for safe retry because the window-thread "
                                 "dispatcher is unavailable (owner={})", owner);
                RetryInspectorTeardown();
            }
            return;
        }
#endif
        ApplyInspectorBackendStateChanged(owner, visible, false);
    }
    void CreateInspectorView(ViewId view) {
        if (st.backend && view) st.backend->CreateInspectorView(view);
    }
    void SetInspectorVisibility(ViewId view, bool visible) {
        if (!st.backend || !view) return;
        if (visible) {
            if (!IsValid(view)) return;
            const ViewId teardown = st.inspectorTeardownPending.load(std::memory_order_acquire);
            if (teardown) {
                logger::warn("[PrismaUI] inspector open rejected while teardown is pending "
                             "(requested={}, teardown={})", view, teardown);
                return;
            }
            const ViewId owner = st.localInspectorOwner.load(std::memory_order_acquire);
            if (owner && owner != view) {
                logger::warn("[PrismaUI] inspector open rejected because another inspector owns input "
                             "(requested={}, owner={})", view, owner);
                return;
            }
            ViewId expectedPending = 0;
            if (!st.localInspectorPending.compare_exchange_strong(expectedPending, view, std::memory_order_acq_rel) &&
                expectedPending != view) {
                logger::warn("[PrismaUI] inspector open rejected because another inspector is pending "
                             "(requested={}, pending={})", view, expectedPending);
                return;
            }
            if (!st.backend->IsReady()) {
                OnInspectorBackendStateChanged(view, false);
                return;
            }
#ifndef PRISMAUI_FO4VR
            VirtualPointer::CancelHeldClick();
#endif
            st.backend->SetInspectorVisibility(view, true);
            if (!st.backend->IsReady() || !IsValid(view)) OnInspectorBackendStateChanged(view, false);
            return;
        }
        const ViewId owner = st.localInspectorOwner.load(std::memory_order_acquire);
        const ViewId pending = st.localInspectorPending.load(std::memory_order_acquire);
        const ViewId teardown = st.inspectorTeardownPending.load(std::memory_order_acquire);
        const bool tracked = owner == view || pending == view || teardown == view;
        if (!st.backend->IsReady()) {
            if (tracked) OnInspectorBackendStateChanged(view, false);
            return;
        }
        if (tracked) {
            ViewId expectedTeardown = 0;
            if (!st.inspectorTeardownPending.compare_exchange_strong(expectedTeardown, view,
                                                                     std::memory_order_acq_rel) &&
                expectedTeardown != view) {
                logger::warn("[PrismaUI] inspector close rejected because another teardown is pending "
                             "(requested={}, teardown={})", view, expectedTeardown);
                return;
            }
            st.inspectorTeardownRetryAt.store(0, std::memory_order_release);
        }
        st.backend->SetInspectorVisibility(view, false);
    }
    bool IsInspectorVisible(ViewId view) { return st.backend && view && st.backend->IsInspectorVisible(view); }
    void SetInspectorBounds(ViewId view, float x, float y, uint32_t width, uint32_t height) {
        if (!st.backend || !view || width < 32 || height < 32) return;
        const double safeX = static_cast<double>(x);
        const double safeY = static_cast<double>(y);
        if (!std::isfinite(safeX) || !std::isfinite(safeY) ||
            safeX < static_cast<double>(std::numeric_limits<int>::lowest()) ||
            safeX > static_cast<double>(std::numeric_limits<int>::max()) ||
            safeY < static_cast<double>(std::numeric_limits<int>::lowest()) ||
            safeY > static_cast<double>(std::numeric_limits<int>::max()))
            return;
        st.backend->SetInspectorBounds(view, x, y, width, height);
    }
    bool ToggleLocalInspector() {
        if (!st.backend || !st.active.load(std::memory_order_acquire) ||
            !DevToolsConfig::Enabled(Utils::PluginIniPath().wstring()))
            return false;
        const ViewId owner = st.localInspectorOwner.load(std::memory_order_acquire);
        if (owner && st.backend->IsInspectorVisible(owner)) {
            SetInspectorVisibility(owner, false);
            return true;
        }
        if (st.localInspectorPending.load(std::memory_order_acquire)) return true;
        const auto views = OrderedVisibleOnscreenViews();
        if (views.empty()) return false;
        const ViewId target = st.focusedView.load(std::memory_order_acquire)
                                  ? st.focusedView.load(std::memory_order_acquire)
                                  : views.back().second;
        if (!target) return false;
        SetInspectorVisibility(target, true);
        return true;
    }
    void BeginInspectorMove(ViewId view, int localX, int localY) {
        if (st.backend) st.backend->BeginInspectorMove(view, localX, localY);
    }
    void BeginInspectorResize(ViewId view, int localX, int localY) {
        if (st.backend) st.backend->BeginInspectorResize(view, localX, localY);
    }
    ViewId UpdateInspectorGesture(int x, int y, int& localX, int& localY) {
        return st.backend ? st.backend->UpdateInspectorGesture(x, y, localX, localY) : 0;
    }
    ViewId InspectorGestureTargetAt(int x, int y, int& localX, int& localY) {
        return st.backend ? st.backend->InspectorGestureTargetAt(x, y, localX, localY) : 0;
    }
    void EndInspectorGesture() {
        st.inspectorPointerCaptureTarget.store(0, std::memory_order_release);
        if (st.backend) st.backend->EndInspectorGesture();
    }
}
