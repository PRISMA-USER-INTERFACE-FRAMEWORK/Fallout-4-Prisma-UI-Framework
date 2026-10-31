#include "Engine/ShellCallFormat.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int g_failures = 0;

void Expect(const std::string& actual, const std::string& expected, const char* what) {
    if (actual != expected) {
        std::cerr << "FAIL: " << what << "\n  expected: " << expected << "\n  actual:   " << actual
                  << '\n';
        ++g_failures;
    }
}

}  // namespace

// Each expectation below is the literal string the hand-written concatenation that the Host ABI
// produced before ShellCall existed. They are the ABI between the Host and shell/src/main.ts:
// changing one silently is a shell call that no longer dispatches.
int main() {
    namespace F = PrismaUI::Engine::ShellCallFormat;

    using ViewId = std::uint64_t;
    const ViewId view = 42;

    Expect(F::Call("showView", view),
           "window.__prismaShell && window.__prismaShell.showView(42);", "showView");
    Expect(F::Call("hideView", view),
           "window.__prismaShell && window.__prismaShell.hideView(42);", "hideView");
    Expect(F::Call("destroyView", view),
           "window.__prismaShell && window.__prismaShell.destroyView(42);", "destroyView");
    Expect(F::Call("focusView", view),
           "window.__prismaShell && window.__prismaShell.focusView(42);", "focusView");
    Expect(F::Call("unfocusView", view),
           "window.__prismaShell && window.__prismaShell.unfocusView(42);", "unfocusView");

    // Two arguments: the ViewId overload and the int overload must not collide.
    Expect(F::Call("setOrder", view, 7),
           "window.__prismaShell && window.__prismaShell.setOrder(42,7);", "setOrder");
    Expect(F::Call("setOrder", view, -3),
           "window.__prismaShell && window.__prismaShell.setOrder(42,-3);", "setOrder negative");

    // No arguments at all -- the separator fold must not emit a stray comma or an empty slot.
    Expect(F::Call("showBootAnimation"),
           "window.__prismaShell && window.__prismaShell.showBootAnimation();", "showBootAnimation");
    Expect(F::Call("hideDock"),
           "window.__prismaShell && window.__prismaShell.hideDock();", "hideDock");

    // Raw-JS argument: a payload the caller already built. It must be interpolated verbatim, NOT
    // quoted -- quoting it would hand the shell a string where it expects an object.
    Expect(F::Call("showDock", std::string("{\"views\":[]}")),
           "window.__prismaShell && window.__prismaShell.showDock({\"views\":[]});", "showDock payload");

    // createView takes a view id and a url the caller has already quoted and escaped. The formatter
    // must not add quotes of its own, or the shell receives a double-quoted url.
    Expect(F::Call("createView", ViewId{9}, std::string("\"Interface/Foo/foo.html\"")),
           "window.__prismaShell && window.__prismaShell.createView(9,\"Interface/Foo/foo.html\");",
           "createView");

    // Large ids must not lose precision through an int overload.
    Expect(F::Call("showView", ViewId{18446744073709551615ull}),
           "window.__prismaShell && window.__prismaShell.showView(18446744073709551615);",
           "showView max ViewId");

    if (g_failures) {
        std::cerr << g_failures << " shell-call format mismatch(es)\n";
        return 1;
    }
    std::cout << "shell_call_format_tests ok\n";
    return 0;
}
