#include "PrismaUI/FocusCursorReassertPolicy.h"

#include <cstdio>

using namespace PrismaUI::FocusCursorReassertPolicy;

static int g_failures = 0;

static void expect(bool condition, const char* what)
{
    std::printf("  [%s] %s\n", condition ? "PASS" : "FAIL", what);
    if (!condition) ++g_failures;
}

int main()
{
    expect(Decide("PauseMenu", true, true) == Action::ClearConstraints,
           "a hidden PauseMenu over a Prisma-owned cursor re-clears the constraints");
    expect(Decide("PauseMenu", false, true) == Action::None,
           "no reassert when Prisma does not own the cursor");
    expect(Decide("PauseMenu", true, false) == Action::None,
           "no reassert when the menu is not hidden");
    expect(Decide("ContainerMenu", true, true) == Action::None,
           "a menu outside the reassert set is ignored");
    expect(Decide("CursorMenu", true, true) == Action::None,
           "the cursor menu itself is not the reassert trigger");
    expect(Decide("PauseMenuExtra", true, true) == Action::None,
           "similar menu names do not match");
    expect(Decide("", true, true) == Action::None, "an empty menu name is ignored");
    expect(IsCursorConstrainingMenu("PauseMenu"), "PauseMenu is the constraining menu");
    expect(!IsCursorConstrainingMenu("pauseMenu"), "menu names are matched exactly");
    expect(!IsCursorConstrainingMenu(""), "an empty name is not the constraining menu");

    if (g_failures == 0) {
        std::printf("\nAll focus-cursor-reassert-policy assertions passed.\n");
        return 0;
    }

    std::printf("\n%d assertion(s) FAILED.\n", g_failures);
    return 1;
}
