#include "PrismaUI/GameLoadWindow.h"

#include <cstdio>
#include <cstdlib>

namespace {

    using Window = PrismaUI::FreezeDiagnostics::GameLoadWindow;

    int g_failures = 0;

    void Check(bool condition, const char* what) {
        if (!condition) {
            std::fprintf(stderr, "FAIL: %s\n", what);
            ++g_failures;
        }
    }

    void OnlyFalseToTrueStartsAWindow() {
        Check(Window::Starts(false, true), "first activation starts a window");
        Check(!Window::Starts(true, true), "a repeated activation does not restart the window");
        Check(!Window::Starts(true, false), "deactivation does not start a window");
        Check(!Window::Starts(false, false), "staying inactive does not start a window");
    }

    void CapIsPerLoad() {
        Check(!Window::BackingOff(false, 0, 1000, 60000), "inactive load never backs off");
        Check(Window::BackingOff(true, 0, 59'999, 60000), "backs off inside the cap");
        Check(!Window::BackingOff(true, 0, 60'000, 60000), "cap releases at 60 s");
        Check(Window::BackingOff(true, 0, 0, 0) == false, "zero cap never backs off");

        bool active = false;
        std::int64_t since = 0;
        auto set = [&](bool value, std::int64_t now) {
            if (Window::Starts(active, value)) since = now;
            if (!value) since = 0;
            active = value;
        };
        set(true, 0);
        set(true, 30'000);
        Check(since == 0, "a second PreLoadGame inside the same load keeps the original start");
        Check(!Window::BackingOff(active, since, 60'000, 60000), "the cap is measured from the first activation");
        set(false, 40'000);
        set(true, 100'000);
        Check(since == 100'000 && Window::BackingOff(active, since, 150'000, 60000),
              "a new load gets its own 60 s cap");
    }

}

int main() {
    OnlyFalseToTrueStartsAWindow();
    CapIsPerLoad();
    if (g_failures) {
        std::fprintf(stderr, "game_load_window_tests: %d failure(s)\n", g_failures);
        return EXIT_FAILURE;
    }
    std::puts("game_load_window_tests passed");
    return EXIT_SUCCESS;
}
