#include "WebInput.h"
#include "WebInputWindow.h"

#include "HwndThreadBootstrap.h"
#include "InputMessageSemantics.h"
#include "MenuCursorDiagnostics.h"
#include "DockBridge.h"
#include "WebRuntime.h"
#include "ViewCursorPolicy.h"
#ifndef PRISMAUI_FO4VR
    #include "GameThreadDispatcher.h"
#endif
#include <commctrl.h>
#include <windowsx.h>

#include <atomic>
#include <cstdint>
#include <mutex>

#include "InputOwnershipPolicy.h"

namespace PrismaUI::WebInput {

void OnWindowDestroyed(HWND hwnd);

    namespace {

        constexpr UINT_PTR kSubclassId = 0x50524953;

        Detail::WindowState g_window;
        HHOOK g_llKeyboardHook = nullptr;
        std::mutex g_installMutex;
        std::atomic<bool> g_capture{false};
        std::atomic<bool> g_swallow{false};
        std::atomic<bool> g_mouseInsideWebView{false};
        std::atomic<uint32_t> g_mouseCaptureMask{0};
        std::atomic<bool> g_nativeMouseCapture{false};

        std::atomic<bool> g_escapeOwned{false};
        std::atomic<bool> g_localInspectorHotkeyDown{false};

        std::atomic<int> g_lastCursorX{0};
        std::atomic<int> g_lastCursorY{0};

        std::mutex g_cursorPolicyMutex;
        ViewCursorPolicy::State g_cursorPolicyState;

        constexpr uint32_t EF_SHIFT = 1u << 1;
        constexpr uint32_t EF_CTRL = 1u << 2;
        constexpr uint32_t EF_ALT = 1u << 3;
        constexpr uint32_t EF_LBUTTON = 1u << 4;
        constexpr uint32_t EF_MBUTTON = 1u << 5;
        constexpr uint32_t EF_RBUTTON = 1u << 6;
        constexpr uint32_t EF_PRISMA_POINTER_CAPTURE = 1u << 30;
        constexpr uint32_t EF_PRISMA_POINTER_CAPTURE_CONTINUES = 1u << 31;

        constexpr int KT_RAWKEYDOWN = 0;
        constexpr int KT_KEYUP = 2;
        constexpr int KT_CHAR = 3;

        constexpr uint32_t kMouseLeft = 1u << 0;
        constexpr uint32_t kMouseMiddle = 1u << 1;
        constexpr uint32_t kMouseRight = 1u << 2;

        bool IsMouseMessage(UINT msg) {
            switch (msg) {
                case WM_MOUSEMOVE:
                case WM_LBUTTONDOWN:
                case WM_LBUTTONDBLCLK:
                case WM_LBUTTONUP:
                case WM_RBUTTONDOWN:
                case WM_RBUTTONUP:
                case WM_MBUTTONDOWN:
                case WM_MBUTTONUP:
                case WM_MOUSEWHEEL:
                case WM_MOUSEHWHEEL:
                    return true;
                default:
                    return false;
            }
        }

        uint32_t MouseButtonBit(UINT msg) {
            switch (msg) {
                case WM_LBUTTONDOWN:
                case WM_LBUTTONDBLCLK:
                case WM_LBUTTONUP:
                    return kMouseLeft;
                case WM_RBUTTONDOWN:
                case WM_RBUTTONUP:
                    return kMouseRight;
                case WM_MBUTTONDOWN:
                case WM_MBUTTONUP:
                    return kMouseMiddle;
                default:
                    return 0;
            }
        }

        bool IsMouseButtonUp(UINT msg) {
            return msg == WM_LBUTTONUP || msg == WM_RBUTTONUP || msg == WM_MBUTTONUP;
        }

        void AcquireMouseCapture(HWND hwnd, uint32_t buttonBit) {
            if (!buttonBit) return;
            const uint32_t previous = g_mouseCaptureMask.fetch_or(buttonBit, std::memory_order_acq_rel);
            if (previous != 0) return;
            SetCapture(hwnd);
            g_nativeMouseCapture.store(GetCapture() == hwnd, std::memory_order_release);
        }

        void ReleaseMouseCapture(HWND hwnd, uint32_t buttonBit) {
            if (!buttonBit) return;
            const uint32_t previous = g_mouseCaptureMask.fetch_and(~buttonBit, std::memory_order_acq_rel);
            if ((previous & ~buttonBit) != 0) return;
            if (g_nativeMouseCapture.exchange(false, std::memory_order_acq_rel) && GetCapture() == hwnd) {
                ReleaseCapture();
            }
        }

        void CancelMouseCapture(HWND hwnd) {
            g_mouseCaptureMask.store(0, std::memory_order_release);
            if (g_nativeMouseCapture.exchange(false, std::memory_order_acq_rel) && GetCapture() == hwnd) {
                ReleaseCapture();
            }
            WebRuntime::EndInspectorGesture();
        }

        uint32_t Modifiers() {
            uint32_t m = 0;
            if (GetKeyState(VK_SHIFT) & 0x8000) m |= EF_SHIFT;
            if (GetKeyState(VK_CONTROL) & 0x8000) m |= EF_CTRL;
            if (GetKeyState(VK_MENU) & 0x8000) m |= EF_ALT;
            if (GetKeyState(VK_LBUTTON) & 0x8000) m |= EF_LBUTTON;
            if (GetKeyState(VK_RBUTTON) & 0x8000) m |= EF_RBUTTON;
            if (GetKeyState(VK_MBUTTON) & 0x8000) m |= EF_MBUTTON;
            return m;
        }

        uint32_t MouseModifiers(WPARAM wParam) {
            uint32_t m = 0;
            if (wParam & MK_SHIFT) m |= EF_SHIFT;
            if (wParam & MK_CONTROL) m |= EF_CTRL;
            if (GetKeyState(VK_MENU) & 0x8000) m |= EF_ALT;
            if (wParam & MK_LBUTTON) m |= EF_LBUTTON;
            if (wParam & MK_MBUTTON) m |= EF_MBUTTON;
            if (wParam & MK_RBUTTON) m |= EF_RBUTTON;
            return m;
        }

        bool CompositorCursorHidden() {
            const bool owned = IsCursorOwned();
            if (!owned) return false;
            std::lock_guard lock(g_cursorPolicyMutex);
            return !ViewCursorPolicy::ShouldDrawCompositorCursor(g_cursorPolicyState, owned);
        }

        bool MapToFocusedOffscreenPage(HWND hwnd, int& x, int& y) {
            const auto view = WebRuntime::GetFocusedView();
            if (!view || !WebRuntime::IsOffscreen(view)) return false;

            int pageW = 0, pageH = 0;
            if (!WebRuntime::GetOffscreenPageSize(view, pageW, pageH) || pageW <= 0 || pageH <= 0) return false;

            RECT rc{};
            if (!GetClientRect(hwnd, &rc)) return false;
            const int winW = rc.right - rc.left;
            const int winH = rc.bottom - rc.top;
            if (winW <= 0 || winH <= 0) return false;

            const auto clamp = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };
            x = clamp(static_cast<int>(static_cast<int64_t>(x) * pageW / winW), 0, pageW - 1);
            y = clamp(static_cast<int>(static_cast<int64_t>(y) * pageH / winH), 0, pageH - 1);
            return true;
        }

        LRESULT CALLBACK SubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR) {
            if (uMsg == WM_NCDESTROY) {
                OnWindowDestroyed(hwnd);
                WebRuntime::ShutdownFrameworkOnWindowThread();
            }
#ifndef PRISMAUI_FO4VR
            if (GameThreadDispatcher::HandleWindowMessage(hwnd, uMsg)) {
                return 0;
            }
#endif

            if (uMsg == WM_KEYDOWN && wParam == VK_F12 && !(lParam & (1 << 30)) &&
                WebRuntime::ToggleLocalInspector()) {
                g_localInspectorHotkeyDown.store(true, std::memory_order_release);
                return 0;
            }
            if (uMsg == WM_KEYUP && wParam == VK_F12 &&
                g_localInspectorHotkeyDown.exchange(false, std::memory_order_acq_rel))
                return 0;

            if (uMsg == WM_CAPTURECHANGED) {
                g_nativeMouseCapture.store(false, std::memory_order_release);
                g_mouseCaptureMask.store(0, std::memory_order_release);
                WebRuntime::EndInspectorGesture();
            }

            if (uMsg == WM_ACTIVATEAPP && !wParam) {
                CancelMouseCapture(hwnd);
                const auto focused = WebRuntime::GetFocusedView();
                logger::info("[WebInput] window deactivated (focused={}, capture={}, swallow={})", focused,
                             g_capture.load(), g_swallow.load());
                const bool inspectorOwnsWindow = WebRuntime::OnWindowActivation(false);
                if (focused && !inspectorOwnsWindow) WebRuntime::Unfocus(focused);
            } else if (uMsg == WM_ACTIVATEAPP && wParam) {
                WebRuntime::OnWindowActivation(true);
            }

            if (!g_capture.load(std::memory_order_acquire) && IsMouseButtonUp(uMsg) &&
                g_nativeMouseCapture.load(std::memory_order_acquire)) {
                CancelMouseCapture(hwnd);
            }

            if (g_capture.load()) {

                if (uMsg == WM_INPUT && g_escapeOwned.load() && g_swallow.load()) {
                    RAWINPUT ri{};
                    UINT sz = sizeof(ri);
                    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &ri, &sz,
                                        sizeof(RAWINPUTHEADER)) != static_cast<UINT>(-1) &&
                        ri.header.dwType == RIM_TYPEKEYBOARD && ri.data.keyboard.VKey == VK_ESCAPE) {
                        return 0;
                    }
                }

                if (uMsg == WM_SETCURSOR) {
                    if (IsCursorOwned()) {
                        SetCursor(nullptr);
                        return TRUE;
                    }
                }

                bool forwarded = false;
                const bool mouseMessage = IsMouseMessage(uMsg);
                int x = 0;
                int y = 0;
                bool rescale = true;
                uint32_t mouseModifiers = 0;

                if (mouseMessage) {
                    x = GET_X_LPARAM(lParam);
                    y = GET_Y_LPARAM(lParam);

                    if (uMsg == WM_MOUSEWHEEL || uMsg == WM_MOUSEHWHEEL) {
                        POINT p{x, y};
                        ScreenToClient(hwnd, &p);
                        x = p.x;
                        y = p.y;
                    }

                    const bool onMesh = MapToFocusedOffscreenPage(hwnd, x, y);
                    rescale = !onMesh;
                    int routeX = x;
                    int routeY = y;
                    if (rescale) WebRuntime::MapClientPointToBrowser(routeX, routeY);
                    if (uMsg == WM_MOUSEMOVE) {
                        g_lastCursorX.store(onMesh ? -1 : x);
                        g_lastCursorY.store(onMesh ? -1 : y);
                    }

                    const uint32_t buttonBit = MouseButtonBit(uMsg);
                    const uint32_t captureMask = g_mouseCaptureMask.load();
                    const bool gestureCaptured = captureMask != 0 && (uMsg == WM_MOUSEMOVE || buttonBit != 0);
                    mouseModifiers = MouseModifiers(wParam);
                    if (captureMask != 0) mouseModifiers |= EF_PRISMA_POINTER_CAPTURE;
                    if (buttonBit != 0 && (captureMask & ~buttonBit) != 0)
                        mouseModifiers |= EF_PRISMA_POINTER_CAPTURE_CONTINUES;

                    if (!gestureCaptured) {
                        const auto owner = InputOwnershipPolicy::OwnerFromFocusedInput(g_swallow.load());
                        const bool focusedViewAcceptsMouse = WebRuntime::ShouldRouteFocusedMouse(routeX, routeY);
                        if (!InputOwnershipPolicy::ShouldRouteMouse(owner, focusedViewAcceptsMouse)) {
                            if (uMsg == WM_MOUSEMOVE && g_mouseInsideWebView.exchange(false)) {
                                WebRuntime::SendMouseMove(x, y, mouseModifiers, true, rescale);
                                forwarded = true;
                            }
                            if (!forwarded) {
                                return DefSubclassProc(hwnd, uMsg, wParam, lParam);
                            }
                        }
                    }
                }

                switch (uMsg) {
                    case WM_MOUSEMOVE:
                        if (!forwarded) {
                            g_mouseInsideWebView.store(true);
                            WebRuntime::SendMouseMove(x, y, mouseModifiers, false, rescale);
                            forwarded = true;
                        }
                        break;
                    case WM_LBUTTONDOWN:
                    case WM_LBUTTONDBLCLK:
                        WebRuntime::SendMouseClick(x, y, 0, false, uMsg == WM_LBUTTONDBLCLK ? 2 : 1, mouseModifiers,
                                                   rescale);
                        AcquireMouseCapture(hwnd, kMouseLeft);
                        forwarded = true;
                        break;
                    case WM_LBUTTONUP:
                        WebRuntime::SendMouseClick(x, y, 0, true, 1, mouseModifiers, rescale);
                        WebRuntime::EndInspectorGesture();
                        ReleaseMouseCapture(hwnd, kMouseLeft);
                        forwarded = true;
                        break;
                    case WM_RBUTTONDOWN:
                        WebRuntime::SendMouseClick(x, y, 2, false, 1, mouseModifiers, rescale);
                        AcquireMouseCapture(hwnd, kMouseRight);
                        forwarded = true;
                        break;
                    case WM_RBUTTONUP:
                        WebRuntime::SendMouseClick(x, y, 2, true, 1, mouseModifiers, rescale);
                        ReleaseMouseCapture(hwnd, kMouseRight);
                        forwarded = true;
                        break;
                    case WM_MBUTTONDOWN:
                        WebRuntime::SendMouseClick(x, y, 1, false, 1, mouseModifiers, rescale);
                        AcquireMouseCapture(hwnd, kMouseMiddle);
                        forwarded = true;
                        break;
                    case WM_MBUTTONUP:
                        WebRuntime::SendMouseClick(x, y, 1, true, 1, mouseModifiers, rescale);
                        ReleaseMouseCapture(hwnd, kMouseMiddle);
                        forwarded = true;
                        break;
                    case WM_MOUSEWHEEL:
                        WebRuntime::SendMouseWheel(x, y, 0, GET_WHEEL_DELTA_WPARAM(wParam), mouseModifiers, rescale);
                        forwarded = true;
                        break;
                    case WM_MOUSEHWHEEL:
                        WebRuntime::SendMouseWheel(x, y, GET_WHEEL_DELTA_WPARAM(wParam), 0, mouseModifiers, rescale);
                        forwarded = true;
                        break;
                    case WM_KEYDOWN:
                    case WM_SYSKEYDOWN:
                        if (wParam == VK_ESCAPE && !g_escapeOwned.load()) break;

                        if (uMsg == WM_SYSKEYDOWN && wParam == VK_F4 && (lParam & (1 << 29))) break;
                        WebRuntime::SendKey(KT_RAWKEYDOWN, static_cast<int>(wParam), static_cast<int>(lParam),
                                            Modifiers(), 0, KeySemantics::IsSystemMessage(uMsg));
                        forwarded = true;
                        break;
                    case WM_KEYUP:
                    case WM_SYSKEYUP:
                        if (wParam == VK_ESCAPE && !g_escapeOwned.load()) break;
                        if (uMsg == WM_SYSKEYUP && wParam == VK_F4 && (lParam & (1 << 29))) break;
                        WebRuntime::SendKey(KT_KEYUP, static_cast<int>(wParam), static_cast<int>(lParam), Modifiers(),
                                            0, KeySemantics::IsSystemMessage(uMsg));
                        forwarded = true;
                        break;
                    case WM_CHAR:
                    case WM_SYSCHAR:
                        WebRuntime::SendKey(KT_CHAR, static_cast<int>(wParam), static_cast<int>(lParam), Modifiers(),
                                            static_cast<uint16_t>(wParam), KeySemantics::IsSystemMessage(uMsg));
                        forwarded = true;
                        break;
                    default:
                        break;
                }

                (void)forwarded;
            }
            return DefSubclassProc(hwnd, uMsg, wParam, lParam);
        }

        bool GameOwnsForeground() {
            const HWND game = static_cast<HWND>(g_window.Get());
            if (!game) return false;
            const HWND fg = GetForegroundWindow();
            if (!fg) return false;
            if (fg == game) return true;
            return GetWindowThreadProcessId(fg, nullptr) == GetWindowThreadProcessId(game, nullptr);
        }

        LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
            if (nCode == HC_ACTION && wParam == WM_SYSKEYDOWN) {
                const auto* kb = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
                if (kb->vkCode == VK_F4 && (kb->flags & LLKHF_ALTDOWN) && GameOwnsForeground()) {
                    if (HWND game = static_cast<HWND>(g_window.Get()))
                        PostMessage(game, WM_SYSCOMMAND, SC_CLOSE, 0);
                }
            }
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
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
#ifndef PRISMAUI_FO4VR
            GameThreadDispatcher::DetachWindow(hwnd);
#endif
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
#ifndef PRISMAUI_FO4VR
        GameThreadDispatcher::DetachWindow(hwnd);
#endif
        if (g_llKeyboardHook) {
            UnhookWindowsHookEx(g_llKeyboardHook);
            g_llKeyboardHook = nullptr;
        }
    }

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
