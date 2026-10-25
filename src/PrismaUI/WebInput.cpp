#include "WebInput.h"
#include "WebInputInternal.h"

#include "HwndThreadBootstrap.h"
#include "MenuCursorDiagnostics.h"
#include "DockBridge.h"
#include "WebRuntime.h"
#include "ViewCursorPolicy.h"
#include "GameThreadDispatcher.h"
#include <commctrl.h>
#include <imm.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>

#include "InputOwnershipPolicy.h"
#include "ImeFocusPolicy.h"

namespace PrismaUI::WebInput {

    namespace {

        constexpr UINT_PTR kSubclassId = 0x50524953;

        HHOOK g_llKeyboardHook = nullptr;
        std::mutex g_installMutex;

        std::mutex g_cursorPolicyMutex;
        ViewCursorPolicy::State g_cursorPolicyState;

        bool CompositorCursorHidden() {
            const bool owned = IsCursorOwned();
            if (!owned) return false;
            std::lock_guard lock(g_cursorPolicyMutex);
            return !ViewCursorPolicy::ShouldDrawCompositorCursor(g_cursorPolicyState, owned);
        }

        ImeFocusPolicy::Association g_imeApplied = ImeFocusPolicy::Association::kGame;
        HIMC g_imeGameContext = nullptr;

        HIMC CaptureGameContext(HWND hwnd) {
            const HIMC context = ImmGetContext(hwnd);
            if (context) ImmReleaseContext(hwnd, context);
            return context;
        }

        void RestoreImeAssociation(HWND hwnd) {
            if (g_imeApplied == ImeFocusPolicy::Association::kGame) return;
            const HIMC context = g_imeGameContext;
            g_imeGameContext = nullptr;
            g_imeApplied = ImeFocusPolicy::Association::kGame;
            ImmAssociateContext(hwnd, context);
        }

    }

    bool Install(HWND gameHwnd) {
        std::lock_guard lock(g_installMutex);
        if (g_window.Installed()) return true;
        if (!gameHwnd) return false;

        DWORD processId = 0;
        const DWORD ownerThread = GetWindowThreadProcessId(gameHwnd, &processId);
        if (!ownerThread || processId != GetCurrentProcessId() || ownerThread != GetCurrentThreadId()) {
            logger::error("[WebInput] refusing cross-thread HWND subclass installation");
            return false;
        }
        if (!g_llKeyboardHook) {
            g_llKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, &LowLevelKeyboardProc, nullptr, 0);
            if (g_llKeyboardHook) {
                logger::info("[WebInput] low-level keyboard hook installed (Alt+F4 guarantee)");
            } else {
                logger::error("[WebInput] SetWindowsHookExW(WH_KEYBOARD_LL) failed, GLE={}", GetLastError());
            }
        }
        if (!SetWindowSubclass(gameHwnd, &SubclassProc, kSubclassId, 0)) {
            const DWORD subclassError = GetLastError();
            if (g_llKeyboardHook) {
                UnhookWindowsHookEx(g_llKeyboardHook);
                g_llKeyboardHook = nullptr;
            }
            g_window.Clear();
            logger::error("[WebInput] SetWindowSubclass failed, GLE={} -- install rolled back", subclassError);
            return false;
        }
#ifndef PRISMAUI_FO4VR
        if (!GameThreadDispatcher::AttachWindow(gameHwnd)) {
            RemoveWindowSubclass(gameHwnd, &SubclassProc, kSubclassId);
            if (g_llKeyboardHook) {
                UnhookWindowsHookEx(g_llKeyboardHook);
                g_llKeyboardHook = nullptr;
            }
            g_window.Clear();
            logger::error(
                "[WebInput] Fallout HWND could not be verified for window-thread dispatch; "
                "install rolled back for retry");
            return false;
        }
#endif
        g_window.Publish(gameHwnd);
        logger::info("[WebInput] WndProc subclass installed on HWND {:p} (window thread)",
                     static_cast<void*>(gameHwnd));
        return true;
    }

    bool QueueInstall(HWND gameHwnd) {
        if (!gameHwnd) return false;
        DWORD processId = 0;
        const DWORD ownerThread = GetWindowThreadProcessId(gameHwnd, &processId);
        if (!ownerThread || processId != GetCurrentProcessId()) return false;
        return HwndThreadBootstrap::Queue(gameHwnd, &Install);
    }

    bool IsInstalled() { return g_window.Installed(); }

#ifndef PRISMAUI_FO4VR
    bool ReattachDispatcher(HWND hwnd) {
        std::lock_guard lock(g_installMutex);
        if (!hwnd || static_cast<HWND>(g_window.Get()) != hwnd) return false;
        return GameThreadDispatcher::RecoverWindow(hwnd);
    }

    bool QueueDispatcherReattach() {
        static std::atomic<int64_t> s_lastRequestMs{0};
        const HWND hwnd = static_cast<HWND>(g_window.Get());
        if (!hwnd || GameThreadDispatcher::IsReady()) return false;
        const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        auto last = s_lastRequestMs.load(std::memory_order_relaxed);
        if (nowMs - last < 1000 || !s_lastRequestMs.compare_exchange_strong(last, nowMs)) return false;
        logger::warn("[WebInput] window-thread dispatcher is not ready on installed HWND {:p}; requesting re-verification",
                     static_cast<void*>(hwnd));
        return HwndThreadBootstrap::Queue(hwnd, &ReattachDispatcher);
    }
#endif

    bool GetClientScreenRect(int& outX, int& outY, int& outWidth, int& outHeight) {
        const HWND hwnd = static_cast<HWND>(g_window.Get());
        if (!hwnd) return false;
        RECT client{};
        if (!GetClientRect(hwnd, &client)) return false;
        const int width = client.right - client.left;
        const int height = client.bottom - client.top;
        if (width <= 0 || height <= 0) return false;
        POINT origin{client.left, client.top};
        if (!ClientToScreen(hwnd, &origin)) return false;
        outX = origin.x;
        outY = origin.y;
        outWidth = width;
        outHeight = height;
        return true;
    }

    void Uninstall() {
        HwndThreadBootstrap::Cancel();
        std::lock_guard lock(g_installMutex);
        const HWND hwnd = static_cast<HWND>(g_window.Get());
        g_window.Clear();
        if (hwnd) {
            RestoreImeAssociation(hwnd);
            GameThreadDispatcher::DetachWindow(hwnd);
            RemoveWindowSubclass(hwnd, &SubclassProc, kSubclassId);
        }
        if (g_llKeyboardHook) {
            UnhookWindowsHookEx(g_llKeyboardHook);
            g_llKeyboardHook = nullptr;
        }
    }

    void OnWindowDestroyed(HWND hwnd) {
        HwndThreadBootstrap::Cancel(hwnd);
        std::lock_guard lock(g_installMutex);
        if (static_cast<HWND>(g_window.Get()) != hwnd) return;
        g_window.Clear();
        RestoreImeAssociation(hwnd);
        GameThreadDispatcher::DetachWindow(hwnd);
        if (g_llKeyboardHook) {
            UnhookWindowsHookEx(g_llKeyboardHook);
            g_llKeyboardHook = nullptr;
        }
    }

    void SyncImeAssociation(HWND hwnd) {
        if (static_cast<HWND>(g_window.Get()) != hwnd) return;
        const bool owned = g_capture.load(std::memory_order_acquire) && g_swallow.load(std::memory_order_acquire);
        const auto action = ImeFocusPolicy::Decide(owned, owned && WebRuntime::PageHasInputFocus(), g_imeApplied);
        if (action == ImeFocusPolicy::Action::kNone) return;
        if (action == ImeFocusPolicy::Action::kRestore) {
            RestoreImeAssociation(hwnd);
            return;
        }
        if (g_imeApplied == ImeFocusPolicy::Association::kGame) g_imeGameContext = CaptureGameContext(hwnd);
        const bool associate = action == ImeFocusPolicy::Action::kAssociate;
        g_imeApplied = associate ? ImeFocusPolicy::Association::kDefault : ImeFocusPolicy::Association::kNone;
        if (associate) {
            ImmAssociateContextEx(hwnd, nullptr, IACE_DEFAULT);
        } else {
            ImmAssociateContext(hwnd, nullptr);
        }
    }

    bool PageOwnsImeUi() { return ImeFocusPolicy::ShowsSystemImeUi(g_imeApplied); }

    void SetEscapeOwned(bool owned) {
        g_escapeOwned.store(owned);
        logger::info("[WebInput] escapeOwned={}", owned);
    }

    void SetCaptureActive(bool active, bool swallow) {
        g_swallow.store(swallow);
        g_capture.store(active);
        g_mouseInsideWebView.store(false);
        WebRuntime::EndInspectorGesture();
        g_mouseCaptureMask.store(0);
        g_pendingSurrogate.Reset();
        const auto focused = active && swallow ? WebRuntime::GetFocusedView() : WebRuntime::ViewId{0};
        {
            std::lock_guard lock(g_cursorPolicyMutex);
            ViewCursorPolicy::OnFocusChanged(g_cursorPolicyState, focused);
        }
        if (focused) {
            MenuCursorDiagnostics::OnFocusedCaptureArmed(focused);
        } else {

            MenuCursorDiagnostics::OnFocusedCaptureReleased();
        }

        if (!focused) g_escapeOwned.store(false);
        if (HWND game = static_cast<HWND>(g_window.Get())) PostMessage(game, WM_NULL, 0, 0);
        logger::info("[WebInput] capture={} swallow={}", active, swallow);
    }

    bool IsCaptureActive() { return g_capture.load(); }

    bool IsCursorOwned() {
        if (!g_capture.load()) return false;
        const auto owner = g_swallow.load() ? InputOwnershipPolicy::CaptureOwner::kFocusedView
                                            : InputOwnershipPolicy::CaptureOwner::kPassiveDock;
        return InputOwnershipPolicy::PrismaOwnsCursor(owner, DockBridge::IsCursorActive());
    }

    bool SetViewCursorPolicy(std::uint64_t view, std::uint32_t policy) {
        if (!view || !WebRuntime::HasFocus(view)) return false;
        if (policy > static_cast<std::uint32_t>(ViewCursorPolicy::Policy::Hidden)) return false;
        {
            std::lock_guard lock(g_cursorPolicyMutex);
            ViewCursorPolicy::SetPolicy(g_cursorPolicyState, view, static_cast<ViewCursorPolicy::Policy>(policy));
        }
        logger::info("[WebInput] view {} compositor cursor policy={}", view, policy);
        return true;
    }

    std::uint32_t GetViewCursorPolicy(std::uint64_t view) {
        if (!view || !WebRuntime::HasFocus(view)) {
            return static_cast<std::uint32_t>(ViewCursorPolicy::Policy::Default);
        }
        std::lock_guard lock(g_cursorPolicyMutex);
        return static_cast<std::uint32_t>(ViewCursorPolicy::GetPolicy(g_cursorPolicyState, view));
    }

    int GetLastCursorX() {
        MenuCursorDiagnostics::OnPresentCursorSample();
        return CompositorCursorHidden() ? -1 : g_lastCursorX.load();
    }
    int GetLastCursorY() { return CompositorCursorHidden() ? -1 : g_lastCursorY.load(); }
    int GetTrackedCursorX() { return g_lastCursorX.load(); }
    int GetTrackedCursorY() { return g_lastCursorY.load(); }

}
