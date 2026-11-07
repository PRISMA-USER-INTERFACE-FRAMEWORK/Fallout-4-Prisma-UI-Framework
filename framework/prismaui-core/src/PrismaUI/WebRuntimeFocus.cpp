#include "WebRuntime.h"
#include "WebRuntimeInternal.h"
#include "VirtualPointer.h"
#include "MenuCursorDiagnostics.h"
#include "GameThreadDispatcher.h"
#include "InputMask.h"
#include "InputOwnershipPolicy.h"
#include "InputRouting.h"
#include "Menus/FocusMenu/FocusMenu.h"
#include "Menus/PauseHold/PauseHold.h"
#include "WebInput.h"
#include "VanillaUISuppressor.h"
namespace PrismaUI::WebRuntime {
    namespace {
        auto& st = Internal::RT();
        using namespace Internal;
        std::mutex focusTransitionMutex;
    }
    void SetViewOwnsEscape(ViewId view, bool owns) {
        if (!view || (owns && !IsValid(view))) return;
        {
            std::lock_guard lock{st.escapeOwnerMutex};
            if (owns)
                st.escapeOwners.insert(view);
            else
                st.escapeOwners.erase(view);
        }
        if (st.focusedView.load() == view && view != 0) WebInput::SetEscapeOwned(owns);
        logger::info("[PrismaUI] SetViewOwnsEscape(view={}, owns={})", view, owns);
    }
    bool HasFocus(ViewId view) { return st.focusedView.load() == view && view != 0; }
    ViewId GetFocusedView() { return st.focusedView.load(); }
    bool PageHasInputFocus() { return st.backend && st.backend->PageHasInputFocus(); }
    void SetViewRole(ViewId view, uint32_t role) {
        if (!view || !IsValid(view)) return;
        std::lock_guard lock(st.roleMutex);
        if (role == 0)
            st.viewRoles.erase(view);
        else
            st.viewRoles[view] = role;
    }
    uint32_t GetViewRole(ViewId view) {
        std::lock_guard lock(st.roleMutex);
        auto it = st.viewRoles.find(view);
        return it == st.viewRoles.end() ? 0u : it->second;
    }
    bool IsAnyPanelVisible(ViewId ignoreView) {
        if (const ViewId focused = st.focusedView.load(); focused && focused != ignoreView) return true;
        std::vector<ViewId> panels;
        {
            std::lock_guard lock(st.roleMutex);
            for (const auto& [view, role] : st.viewRoles) {
                if (role == 2 && view != ignoreView) panels.push_back(view);
            }
        }
        for (const ViewId view : panels) {
            if (IsValid(view) && !IsHidden(view)) return true;
        }
        return false;
    }
    void SetFocusedInputMode(ViewId view, InputRegionPolicy::CaptureMode mode) {
        std::lock_guard lock(st.inputRegionMutex);
        InputRegionPolicy::SetFocusedView(st.inputRegionState, view);
        InputRegionPolicy::SetMode(st.inputRegionState, mode);
    }
    void ClearFocusedInputMode(ViewId view) {
        std::lock_guard lock(st.inputRegionMutex);
        InputRegionPolicy::ClearFocusedView(st.inputRegionState, view);
    }
    bool Internal::FocusImpl(ViewId view, bool pauseGame, bool disableFocusMenu, InputRegionPolicy::CaptureMode mode) {
        if (!st.backend || !st.active.load()) {
            logger::warn("[PrismaUI] Focus(view={}) requested before the web backend is active -- ignoring", view);
            return false;
        }
        if (!view || !IsValid(view)) {
            logger::warn("[PrismaUI] Focus(view={}) -- not a live view, ignoring", view);
            return false;
        }
        const auto health = st.backend->GetViewHealth(view);
        if (health != PrismaUI::Web::ViewHealth::DomReady && health != PrismaUI::Web::ViewHealth::Live) {
            logger::warn("[PrismaUI] Focus(view={}) -- view is not DOM-ready (health={}), ignoring", view,
                         static_cast<int>(health));
            return false;
        }
#ifndef PRISMAUI_FO4VR
        if (!GameThreadDispatcher::IsReady()) {
            logger::error(
                "[PrismaUI] Focus(view={}) refused: window-thread dispatcher is not ready, "
                "so pause/input cannot be released safely",
                view);
            return false;
        }
#endif
        WarnIfRoleUndeclared(view);
        if (!st.focusSession.load(std::memory_order_acquire) &&
            !InputMask::Acquire(InputMask::Owner::kFocus))
            return false;
        const ViewId prevFocused = st.focusedView.load();
        const bool inspectorOwnsBackendFocus =
            st.localInspectorOwner.load(std::memory_order_acquire) != 0 ||
            st.localInspectorPending.load(std::memory_order_acquire) != 0 ||
            st.inspectorTeardownPending.load(std::memory_order_acquire) != 0;
        if (!inspectorOwnsBackendFocus) {
            VirtualPointer::CancelHeldClick();
            if (prevFocused && prevFocused != view) {
                st.backend->UnfocusView(prevFocused);
            }
            st.backend->FocusView(view);
        }
        {
            std::lock_guard lock{ focusTransitionMutex };
            const bool wasUnfocused = st.focusedView.load() == 0;
            st.focusedView.store(view);
            if (wasUnfocused) PrismaUI::VanillaUISuppressor::SetPrismaUIFocusActive(true);
        }
        st.activationRestoreView.store(0, std::memory_order_release);
        st.focusPauseGame.store(pauseGame, std::memory_order_release);
        st.focusDisableFocusMenu.store(disableFocusMenu, std::memory_order_release);
        if (prevFocused != view) {
            st.focusEpisodeStartMs.store(HealthNowMs());
            const std::uint64_t focusGeneration =
                static_cast<std::uint64_t>(st.focusGeneration.fetch_add(1, std::memory_order_relaxed)) + 1;
            MenuCursorDiagnostics::OnFocusEpisodeBegan(view, focusGeneration);
        }
        SetFocusedInputMode(view, mode);
        if (inspectorOwnsBackendFocus) EndInspectorGesture();
        SetInputTargetView(InputRouting::DestinationForFocusedView(view));
        WebInput::SetCaptureActive(true, true);
        WebInput::SetEscapeOwned(ViewOwnsEscape(view));
        if (auto* menuCursor = RE::MenuCursor::GetSingleton()) {
            menuCursor->ClearConstraints();
        }
        if (!st.focusSession.exchange(true)) {
            if (auto* menuCursor = RE::MenuCursor::GetSingleton()) {
                menuCursor->RegisterCursor();
            }
        }
        MenuCursorDiagnostics::OnFocusRegistered(
            view, static_cast<std::uint64_t>(st.focusGeneration.load(std::memory_order_acquire)));
        PauseHold::Set(pauseGame);
        if (disableFocusMenu) {
            FocusMenu::Close();
        } else {
            FocusMenu::Open(pauseGame);
        }
        return true;
    }
    bool Focus(ViewId view, bool pauseGame, bool disableFocusMenu) {
        return FocusImpl(view, pauseGame, disableFocusMenu, InputRegionPolicy::CaptureMode::Full);
    }
    bool FocusOverlay(ViewId view, bool pauseGame, bool disableFocusMenu) {
        return FocusImpl(view, pauseGame, disableFocusMenu, InputRegionPolicy::CaptureMode::OverlayRegions);
    }
    void Unfocus(ViewId view) {
        if (!view || st.focusedView.load() != view) {
            if (st.backend && view) st.backend->UnfocusView(view);
            return;
        }
        VirtualPointer::CancelHeldClick();
        if (st.backend) st.backend->UnfocusView(view);
        {
            std::lock_guard lock{ focusTransitionMutex };
            ViewId expected = view;
            const bool released = st.focusedView.compare_exchange_strong(expected, 0);
            if (released && st.focusedView.load() == 0) PrismaUI::VanillaUISuppressor::SetPrismaUIFocusActive(false);
        }
        if (st.focusedView.load() == 0) MenuCursorDiagnostics::OnFocusEpisodeEnded();
        if (st.focusedView.load() == 0 && st.localInspectorOwner.load(std::memory_order_acquire)) {
            ClearFocusedInputMode(view);
            EndInspectorGesture();
            SetInputTargetView(0);
            WebInput::SetCaptureActive(true, true);
            SyncVanillaCursorVisibility();
            return;
        }
        if (st.focusedView.load() == 0 && st.focusSession.exchange(false)) {
            ClearFocusedInputMode(view);
            const auto owner = InputOwnershipPolicy::AfterFocusedViewRelease(st.dockVisible.load());
            SetInputTargetView(owner == InputOwnershipPolicy::CaptureOwner::kPassiveDock
                                   ? st.dockView.load(std::memory_order_acquire)
                                   : 0);
            WebInput::SetCaptureActive(owner != InputOwnershipPolicy::CaptureOwner::kNone, false);
            SyncVanillaCursorVisibility();
            FocusMenu::Close();
            PauseHold::Set(false);
            (void)InputMask::Release(InputMask::Owner::kFocus);
            if (auto* menuCursor = RE::MenuCursor::GetSingleton()) {
                menuCursor->UnregisterCursor();
            }
        }
    }
}
