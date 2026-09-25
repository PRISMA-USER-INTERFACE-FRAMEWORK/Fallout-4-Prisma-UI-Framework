#include "PCH.h"
#include "HwndThreadBootstrap.h"

#include <mutex>

namespace PrismaUI::HwndThreadBootstrap {
namespace {

std::mutex g_mutex;
HHOOK g_hook = nullptr;
HWND g_hwnd = nullptr;
DWORD g_thread = 0;
UINT g_message = 0;
InstallCallback g_callback = nullptr;

UINT MessageId() noexcept
{
    static const UINT id = ::RegisterWindowMessageW(L"PrismaUI_F4.HwndThreadBootstrap.v1");
    return id;
}

void ClearLocked() noexcept
{
    const HHOOK hook = g_hook;
    g_hook = nullptr;
    g_hwnd = nullptr;
    g_thread = 0;
    g_message = 0;
    g_callback = nullptr;
    if (hook) ::UnhookWindowsHookEx(hook);
}

LRESULT CALLBACK HookProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code >= 0 && lParam) {
        const auto* event = reinterpret_cast<const CWPSTRUCT*>(lParam);
        std::lock_guard lock(g_mutex);
        if (g_hook && event->hwnd == g_hwnd &&
            (event->message == g_message || event->message == WM_NCDESTROY) &&
            ::GetCurrentThreadId() == g_thread) {
            DWORD process = 0;
            const DWORD owner = ::GetWindowThreadProcessId(g_hwnd, &process);
            if (owner == g_thread && process == ::GetCurrentProcessId()) {
                const HWND target = g_hwnd;
                const InstallCallback callback = g_callback;
                const UINT message = event->message;
                ClearLocked();
                if (message != WM_NCDESTROY && callback) callback(target);
            }
        }
    }

    return ::CallNextHookEx(nullptr, code, wParam, lParam);
}

}

bool Queue(HWND hwnd, InstallCallback callback) noexcept
{
    if (!hwnd || !callback) return false;

    DWORD process = 0;
    const DWORD owner = ::GetWindowThreadProcessId(hwnd, &process);
    if (!owner || process != ::GetCurrentProcessId()) return false;

    if (owner == ::GetCurrentThreadId()) {
        Cancel(hwnd);
        return callback(hwnd);
    }

    std::lock_guard lock(g_mutex);
    if (g_hook && g_hwnd == hwnd && g_thread == owner && g_callback == callback) return true;
    if (g_hook) ClearLocked();

    const UINT message = MessageId();
    if (!message) return false;

    const HHOOK hook = ::SetWindowsHookExW(WH_CALLWNDPROC, &HookProc, nullptr, owner);
    if (!hook) return false;

    g_hook = hook;
    g_hwnd = hwnd;
    g_thread = owner;
    g_message = message;
    g_callback = callback;
    if (::PostMessageW(hwnd, message, 0, 0)) return true;

    ClearLocked();
    return false;
}

void Cancel(HWND hwnd) noexcept
{
    std::lock_guard lock(g_mutex);
    if (g_hook && (!hwnd || g_hwnd == hwnd)) ClearLocked();
}

}
