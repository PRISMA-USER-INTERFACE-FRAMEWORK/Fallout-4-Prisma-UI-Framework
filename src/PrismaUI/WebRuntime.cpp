#include "WebRuntime.h"
#include "WebRuntimeInternal.h"
#ifndef PRISMAUI_FO4VR
#include "VirtualPointer.h"
#endif
#include <Windows.h>
#include "InputOwnershipPolicy.h"
#include "WebInput.h"
#include "OffscreenFrames.h"
#include "ModelPreview.h"
#include "TextureOverlay.h"
#include <algorithm>
#include <chrono>
#include <mutex>
#include <set>
#include <utility>
#include <vector>
namespace PrismaUI::WebRuntime {
    namespace {
        auto& st = Internal::RT();
        using namespace Internal;
    }
    RuntimeState& Internal::RT() {
        static RuntimeState s;
        return s;
    }
    void Internal::ClientToBackbuffer(int& x, int& y) {
        const int cw = st.clientW.load(), ch = st.clientH.load();
        const int bw = st.bbW.load(), bh = st.bbH.load();
        if (cw > 0 && ch > 0 && bw > 0 && bh > 0 && (cw != bw || ch != bh)) {
            x = static_cast<int>(std::lround(static_cast<double>(x) * bw / cw));
            y = static_cast<int>(std::lround(static_cast<double>(y) * bh / ch));
        }
    }
    int64_t Internal::HealthNowMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }
    void Internal::WarnIfRoleUndeclared(ViewId view) {
        {
            std::lock_guard lock(st.roleMutex);
            if (st.viewRoles.count(view) != 0) return;
            if (!st.roleWarnedViews.insert(view).second) return;
        }
        logger::warn(
            "[PrismaUI] view {} took focus without declaring a role -- call "
            "SetViewRole(view, ViewRole::kPanel) (or kWidget) so other plugins' IsAnyPanelVisible() can "
            "see it. Undeclared views are never counted, so this one can be on screen while the framework "
            "still reports that nothing is in the way.",
            view);
    }
    bool Internal::ViewOwnsEscape(ViewId view) {
        std::lock_guard lock{st.escapeOwnerMutex};
        return st.escapeOwners.count(view) != 0;
    }
    bool Internal::IsRuntimeViewVisible(ViewId view) {
        std::lock_guard lock{st.viewPresentationMutex};
        const auto it = st.viewPresentation.find(view);
        return it != st.viewPresentation.end() && it->second.visible;
    }
    int Internal::RuntimeViewOrder(ViewId view) {
        std::lock_guard lock{st.viewPresentationMutex};
        const auto it = st.viewPresentation.find(view);
        return it == st.viewPresentation.end() ? 0 : it->second.order;
    }
    std::vector<std::pair<int, ViewId>> Internal::OrderedVisibleOnscreenViews() {
        std::vector<std::pair<int, ViewId>> views;
        {
            std::lock_guard lock{st.viewPresentationMutex};
            views.reserve(st.viewPresentation.size());
            for (const auto& [view, state] : st.viewPresentation) {
                if (state.visible && !OffscreenFrames::IsTracked(view)) views.emplace_back(state.order, view);
            }
        }
        std::sort(views.begin(), views.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.first == rhs.first ? lhs.second < rhs.second : lhs.first < rhs.first;
        });
        return views;
    }
    bool Internal::HasVisibleOnscreenView() {
        std::lock_guard lock{st.viewPresentationMutex};
        return std::any_of(st.viewPresentation.begin(), st.viewPresentation.end(), [](const auto& entry) {
            return entry.second.visible && !OffscreenFrames::IsTracked(entry.first);
        });
    }
    void Internal::ForgetViewShell(ViewId view) {
        if (!view) return;
#ifndef PRISMAUI_FO4VR
        VirtualPointer::DiscardHeldClick(view);
#endif
        {
            std::lock_guard<std::mutex> lock(st.cbMutex);
            for (auto it = st.domReadyByToken.begin(); it != st.domReadyByToken.end();) {
                if (it->second.view == view)
                    it = st.domReadyByToken.erase(it);
                else
                    ++it;
            }
            for (auto it = st.resultByToken.begin(); it != st.resultByToken.end();) {
                if (it->second.view == view)
                    it = st.resultByToken.erase(it);
                else
                    ++it;
            }
            for (auto it = st.listenerByToken.begin(); it != st.listenerByToken.end();) {
                if (it->second.view == view)
                    it = st.listenerByToken.erase(it);
                else
                    ++it;
            }
            for (auto it = st.consoleByToken.begin(); it != st.consoleByToken.end();) {
                if (it->second.view == view)
                    it = st.consoleByToken.erase(it);
                else
                    ++it;
            }
        }
        {
            std::lock_guard lock(st.offscreenSizeMutex);
            st.offscreenSizeMap.erase(view);
        }
        {
            std::lock_guard lock(st.roleMutex);
            st.viewRoles.erase(view);
            st.roleWarnedViews.erase(view);
        }
        {
            std::lock_guard lock{st.escapeOwnerMutex};
            st.escapeOwners.erase(view);
        }
        {
            std::lock_guard lock(st.inputRegionMutex);
            InputRegionPolicy::OnViewDestroyed(st.inputRegionState, view);
        }
        ViewId expectedInputTarget = view;
        st.inputTargetView.compare_exchange_strong(expectedInputTarget, 0, std::memory_order_acq_rel);
        ViewId expectedInspectorMouse = view;
        st.inspectorMouseTarget.compare_exchange_strong(expectedInspectorMouse, 0, std::memory_order_acq_rel);
        {
            std::lock_guard lock{st.viewPresentationMutex};
            st.viewPresentation.erase(view);
        }
        ViewId expectedBoot = view;
        st.bootView.compare_exchange_strong(expectedBoot, 0, std::memory_order_acq_rel);
        ViewId expectedDock = view;
        st.dockView.compare_exchange_strong(expectedDock, 0, std::memory_order_acq_rel);
        OffscreenFrames::Forget(view);
    }
    bool Internal::SetVanillaCursorVisible(bool visible) {
        if (auto* ui = RE::UI::GetSingleton()) {
            if (auto cur = ui->GetMenu("CursorMenu"); cur && cur->uiMovie) {
                cur->uiMovie->SetVisible(visible);
                return true;
            }
        }
        return false;
    }
    void Internal::SyncVanillaCursorVisibility() {
        const auto owner = WebInput::IsCursorOwned() ? InputOwnershipPolicy::CaptureOwner::kFocusedView
                                                     : InputOwnershipPolicy::CaptureOwner::kNone;
        const auto action =
            InputOwnershipPolicy::NextCursorVisibilityAction(owner, st.vanillaCursorHiddenByPrisma.load());
        if (action == InputOwnershipPolicy::CursorVisibilityAction::kHideVanilla) {
            bool expected = false;
            if (st.vanillaCursorHiddenByPrisma.compare_exchange_strong(expected, true) &&
                !SetVanillaCursorVisible(false)) {
                st.vanillaCursorHiddenByPrisma.store(false);
            }
        } else if (action == InputOwnershipPolicy::CursorVisibilityAction::kRestoreVanilla) {
            if (st.vanillaCursorHiddenByPrisma.exchange(false)) {
                Internal::SetVanillaCursorVisible(true);
            }
        }
    }
    void Internal::DomReadyTramp(PrismaUI::Web::ViewId view, uint64_t token) {
        std::function<void(ViewId)> fn;
        ViewId registeredView = 0;
        {
            std::lock_guard<std::mutex> lock(st.cbMutex);
            auto it = st.domReadyByToken.find(token);
            if (it != st.domReadyByToken.end()) {
                registeredView = it->second.view;
                fn = std::move(it->second.callback);
                if (!view && !registeredView) st.failedDomReadyTokens.insert(token);
                st.domReadyByToken.erase(it);
            }
        }
        if (!view) {
            if (registeredView) {
                ModelPreview::OnViewDestroyed(registeredView);
                TextureOverlay::OnViewDestroyed(registeredView);
                if (st.focusedView.load(std::memory_order_acquire) == registeredView) {
                    Unfocus(registeredView);
                }
                Internal::ForgetViewShell(registeredView);
            }
            return;
        }
        if (fn) fn(view);
    }
    void Internal::ResultTramp(std::string json, uint64_t token) {
        std::function<void(std::string)> fn;
        {
            std::lock_guard<std::mutex> lock(st.cbMutex);
            auto it = st.resultByToken.find(token);
            if (it != st.resultByToken.end()) {
                fn = std::move(it->second.callback);
                st.resultByToken.erase(it);
            }
        }
        if (fn) fn(std::move(json));
    }
    void Internal::ListenerTramp(std::string arg, uint64_t token) {
        std::function<void(std::string)> fn;
        {
            std::lock_guard<std::mutex> lock(st.cbMutex);
            auto it = st.listenerByToken.find(token);
            if (it != st.listenerByToken.end()) fn = it->second.callback;
        }
        if (fn) fn(std::move(arg));
    }
    void Internal::ConsoleTramp(int level, std::string message, std::string source, int line, uint64_t token) {
        std::function<void(int, std::string, std::string, int)> fn;
        {
            std::lock_guard<std::mutex> lock(st.cbMutex);
            auto it = st.consoleByToken.find(token);
            if (it != st.consoleByToken.end()) fn = it->second.callback;
        }
        if (fn) fn(level, std::move(message), std::move(source), line);
    }
}
