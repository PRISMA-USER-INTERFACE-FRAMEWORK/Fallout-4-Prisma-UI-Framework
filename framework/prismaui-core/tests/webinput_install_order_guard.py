#!/usr/bin/env python3
"""WebInput must not publish the HWND until the setup it advertises is complete.

IsInstalled/GetClientScreenRect report readiness to the virtual pointer. If the HWND is published
before GameThreadDispatcher::AttachWindow succeeds (flat), a caller can be handed a window that the
very next step rolls back. Publication therefore happens after both acquire steps, and teardown
unpublishes before it detaches/removes the subclass so a reader that cannot tolerate the window
stops seeing it immediately.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src" / "PrismaUI"


def read(name: str) -> str:
    return (SRC / name).read_text(encoding="utf-8", errors="replace")


def body(source: str, signature: str) -> str:
    cursor = 0
    while True:
        start = source.find(signature, cursor)
        if start < 0:
            raise AssertionError(f"no definition found: {signature}")
        brace = source.find("{", start)
        semicolon = source.find(";", start)
        if brace != -1 and (semicolon == -1 or brace < semicolon):
            return block(source, brace)
        cursor = start + len(signature)


def block(source: str, opening: int) -> str:
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError("unclosed block")


source = read("WebInput.cpp")
window_state = read("WebInputWindow.h")


def fail(message: str) -> None:
    print(f"FAIL: {message}")
    raise SystemExit(1)


install = body(source, "bool Install(HWND gameHwnd)")
subclass = install.index("SetWindowSubclass(gameHwnd, &SubclassProc, kSubclassId, 0)")
attach = install.index("GameThreadDispatcher::AttachWindow(gameHwnd)")
publish = install.index("g_window.Publish(gameHwnd)")
endif = install.index("#endif")
if not subclass < attach < endif < publish:
    fail("flat install must publish only after SetWindowSubclass and AttachWindow both succeed")

attach_guard = install.index("if (!GameThreadDispatcher::AttachWindow(gameHwnd))")
attach_failure = block(install, install.index("{", attach_guard))
if "g_window.Clear();" not in attach_failure or "return false;" not in attach_failure:
    fail("a failed dispatcher attach must roll back and stay unpublished")
if "Publish" in attach_failure:
    fail("the dispatcher-attach failure path must not publish the HWND")

subclass_guard = install.index("if (!SetWindowSubclass(gameHwnd, &SubclassProc, kSubclassId, 0))")
subclass_failure = block(install, install.index("{", subclass_guard))
if "g_window.Clear();" not in subclass_failure or "return false;" not in subclass_failure:
    fail("a failed subclass install must stay unpublished")
if "Publish" in subclass_failure:
    fail("the subclass failure path must not publish the HWND")
if install.count("g_window.Publish(") != 1:
    fail("Install must have exactly one publish point, after every acquire step")

uninstall = body(source, "void Uninstall()")
snapshot = uninstall.index("const HWND hwnd = static_cast<HWND>(g_window.Get());")
unpublish = uninstall.index("g_window.Clear();")
detach = uninstall.index("GameThreadDispatcher::DetachWindow(hwnd)")
remove = uninstall.index("RemoveWindowSubclass(hwnd, &SubclassProc, kSubclassId)")
unhook = uninstall.index("UnhookWindowsHookEx(g_llKeyboardHook)")
if not snapshot < unpublish < detach < remove:
    fail("Uninstall must snapshot and unpublish before detach/subclass removal")
if unpublish > unhook:
    fail("Uninstall must unpublish before hook teardown")

destroyed = body(source, "void OnWindowDestroyed(HWND hwnd)")
verify = destroyed.index("static_cast<HWND>(g_window.Get()) != hwnd")
unpublish = destroyed.index("g_window.Clear();")
detach = destroyed.index("GameThreadDispatcher::DetachWindow(hwnd)")
unhook = destroyed.index("UnhookWindowsHookEx(g_llKeyboardHook)")
if not verify < unpublish < detach < unhook:
    fail("OnWindowDestroyed must verify, unpublish, then detach/unhook")

if window_state.count("std::atomic<") != 1:
    fail("WindowState must stay a single atomic handle")
if "installed_" in window_state:
    fail("WindowState must not reintroduce a separate installed flag")
if "void* Get() const noexcept { return handle_.load(std::memory_order_acquire); }" not in window_state:
    fail("WindowState::Get must be a single acquire load of the published handle")

print("WebInput install order guard passed")
