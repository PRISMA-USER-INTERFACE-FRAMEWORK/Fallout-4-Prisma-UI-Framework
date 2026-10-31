#include "PrismaUI/FreezeDiagnostics.h"
#include "PrismaUI/GameThreadDispatcher.h"
#include "PrismaUI/WebRuntime.h"

#include <cstdio>
#include <thread>

namespace PrismaUI::FreezeDiagnostics {
void MarkStage(Lane, Stage, uint64_t, uint64_t, int64_t) noexcept {}
void MarkIdle(Lane) noexcept {}
void BeginDropViewWait(uint64_t) noexcept {}
void EndDropViewWait() noexcept {}
}
namespace PrismaUI::WebRuntime {
bool IsValid(ViewId) { return true; }
}

namespace GTD = PrismaUI::GameThreadDispatcher;

static int g_failures = 0;

static void expect(bool ok, const char* what)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_failures;
}

static void pump()
{
    MSG msg;
    while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (!GTD::HandleWindowMessage(msg.hwnd, msg.message)) ::DispatchMessageW(&msg);
    }
}

static void failFromForeignThread(HWND hwnd)
{
    std::thread([hwnd] { (void)GTD::AttachWindow(hwnd); }).join();
}

int main()
{
    const HWND hwnd = ::CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
    expect(hwnd != nullptr, "test window created");
    expect(GTD::AttachWindow(hwnd), "first attach on the window thread succeeds");

    int staleRan = 0;
    int freshRan = 0;
    expect(GTD::Dispatch([&] { ++staleRan; }), "pre-failure callback accepted");

    failFromForeignThread(hwnd);
    expect(!GTD::IsReady(), "cross-thread attach fails the dispatcher closed");
    expect(!GTD::Dispatch([&] { ++staleRan; }), "dispatch is rejected while failed");

    expect(!GTD::AttachWindow(hwnd), "ordinary AttachWindow on the window thread cannot recover");
    expect(!GTD::IsReady(), "dispatcher stays failed after ordinary AttachWindow");
    pump();
    expect(staleRan == 0, "stale callbacks do not run after ordinary AttachWindow");

    std::thread([hwnd] { expect(!GTD::RecoverWindow(hwnd), "recovery off the window thread is refused"); }).join();
    expect(!GTD::IsReady(), "refused recovery keeps the dispatcher failed");

    expect(GTD::RecoverWindow(hwnd), "verified window-thread recovery succeeds");
    expect(GTD::IsReady(), "dispatcher is ready after verified recovery");
    expect(GTD::Dispatch([&] { ++freshRan; }), "new-generation callback accepted");
    pump();
    expect(staleRan == 0, "stale-generation callbacks never run after recovery");
    expect(freshRan == 1, "new-generation callback runs after recovery");
    expect(GTD::RecoverWindow(hwnd), "recovery on a healthy dispatcher is a no-op success");

    ::DestroyWindow(hwnd);
    std::printf("%s (%d failures)\n", g_failures ? "FAILED" : "OK", g_failures);
    return g_failures ? 1 : 0;
}
