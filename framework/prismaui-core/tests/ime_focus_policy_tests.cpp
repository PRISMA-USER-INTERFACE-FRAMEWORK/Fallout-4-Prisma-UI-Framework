#include "PrismaUI/ImeFocusPolicy.h"

#include <cstdio>

using namespace PrismaUI::ImeFocusPolicy;

static int g_failures = 0;

static void expect(bool condition, const char* what)
{
    std::printf("  [%s] %s\n", condition ? "PASS" : "FAIL", what);
    if (!condition) ++g_failures;
}

int main()
{
    expect(Decide(false, false, Association::kGame) == Action::kNone,
           "keyboard not owned, game association untouched: no Win32 call");
    expect(Decide(false, true, Association::kGame) == Action::kNone,
           "page input focus ignored while Prisma does not own the keyboard");
    expect(Decide(false, false, Association::kDefault) == Action::kRestore,
           "keyboard released after default association: restore the game's context");
    expect(Decide(false, true, Association::kNone) == Action::kRestore,
           "keyboard released after dissociation: restore the game's context");
    expect(Decide(true, true, Association::kGame) == Action::kAssociate,
           "input focus while Prisma owns the keyboard: associate the default context");
    expect(Decide(true, true, Association::kNone) == Action::kAssociate,
           "input focus regained: associate the default context");
    expect(Decide(true, true, Association::kDefault) == Action::kNone,
           "input focus unchanged: no Win32 call");
    expect(Decide(true, false, Association::kGame) == Action::kDissociate,
           "no input focus while Prisma owns the keyboard: dissociate");
    expect(Decide(true, false, Association::kDefault) == Action::kDissociate,
           "input focus lost: dissociate");
    expect(Decide(true, false, Association::kNone) == Action::kNone,
           "no input focus unchanged: no Win32 call");

    expect(ShowsSystemImeUi(Association::kDefault),
           "default association: the page owns the system IME UI");
    expect(!ShowsSystemImeUi(Association::kGame),
           "game association: the game owns the IME UI");
    expect(!ShowsSystemImeUi(Association::kNone),
           "no association: no IME UI is shown by Prisma");

    if (g_failures == 0) {
        std::printf("\nAll IME focus policy assertions passed.\n");
        return 0;
    }
    std::printf("\n%d assertion(s) FAILED.\n", g_failures);
    return 1;
}
