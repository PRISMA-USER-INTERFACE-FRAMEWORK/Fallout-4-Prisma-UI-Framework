#include "PrismaUI/PresentationScalePolicy.h"

#include <cstdio>

using namespace PrismaUI::PresentationScalePolicy;

static int g_failures = 0;

static void expect(bool condition, const char* what)
{
    std::printf("  [%s] %s\n", condition ? "PASS" : "FAIL", what);
    if (!condition) ++g_failures;
}

int main()
{
    expect(IsIdentity(For(1505, 847, 1505, 847)), "equal target keeps view coordinates unchanged");
    expect(IsIdentity(For(0, 0, 2560, 1440)), "missing view size keeps view coordinates unchanged");

    const Scale upscaled = For(1505, 847, 2560, 1440);
    expect(!IsIdentity(upscaled), "window-sized target requires scaling");
    expect(upscaled.x > 1.69F && upscaled.x < 1.71F, "width scale matches 2560/1505");
    expect(upscaled.y > 1.69F && upscaled.y < 1.71F, "height scale matches 1440/847");

    const Scale widthOnly = For(1505, 847, 2560, 847);
    expect(widthOnly.x > 1.69F && widthOnly.x < 1.71F, "width-only mismatch scales width");
    expect(widthOnly.y == 1.0F, "width-only mismatch keeps height scale");

    const Scale downscaled = For(2560, 1440, 1505, 847);
    expect(downscaled.x < 0.59F && downscaled.x > 0.58F, "smaller target scales down");

    if (g_failures == 0) {
        std::printf("\nAll presentation-scale-policy assertions passed.\n");
        return 0;
    }

    std::printf("\n%d assertion(s) FAILED.\n", g_failures);
    return 1;
}
