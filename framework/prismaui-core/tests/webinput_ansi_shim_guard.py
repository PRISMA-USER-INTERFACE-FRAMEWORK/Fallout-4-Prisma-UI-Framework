#!/usr/bin/env python3
"""The ANSI shim must sit on top of an ANSI window's WndProc only after the Unicode subclass exists,
and teardown must restore the saved proc only while the shim is still the window's top proc.
"""
from pathlib import Path
import re

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


def fail(message: str) -> None:
    print(f"FAIL: {message}")
    raise SystemExit(1)


source = read("WebInput.cpp")
install = body(source, "bool Install(HWND gameHwnd)")
unicode_check = install.index("IsWindowUnicode(gameHwnd)")
subclass = install.index("SetWindowSubclass(gameHwnd, &SubclassProc, kSubclassId, 0)")
shim = install.index("SetWindowLongPtrA(gameHwnd, GWLP_WNDPROC")
attach = install.index("GameThreadDispatcher::AttachWindow(gameHwnd)")
if not unicode_check < subclass < shim < attach:
    fail("Install must check IsWindowUnicode before SetWindowSubclass and install the ANSI shim after it")

attach_guard = install.index("if (!GameThreadDispatcher::AttachWindow(gameHwnd))")
attach_failure = block(install, install.index("{", attach_guard))
if "RemoveAnsiShim(gameHwnd);" not in attach_failure:
    fail("a failed dispatcher attach must remove the ANSI shim before the subclass")
if attach_failure.index("RemoveAnsiShim(gameHwnd);") > attach_failure.index("RemoveWindowSubclass(gameHwnd"):
    fail("the ANSI shim must come off before RemoveWindowSubclass on the attach failure path")

shim_proc = body(source, "LRESULT CALLBACK AnsiShimProc(")
if "CallWindowProcA(prev, hwnd, uMsg, wParam, lParam)" not in shim_proc:
    fail("AnsiShimProc must forward through CallWindowProcA")
if re.search(r"\bprev\s*\(", shim_proc) or re.search(r"\bg_ansiShimPrev\s*\(", source):
    fail("AnsiShimProc must never call the saved proc directly")

remove = body(source, "void RemoveAnsiShim(HWND hwnd)")
if "GetWindowLongPtrA(hwnd, GWLP_WNDPROC)" not in remove or "&AnsiShimProc" not in remove:
    fail("RemoveAnsiShim must restore the saved proc only when GetWindowLongPtrA shows the shim on top")
if remove.index("GetWindowLongPtrA(hwnd, GWLP_WNDPROC)") > remove.index("SetWindowLongPtrA("):
    fail("RemoveAnsiShim must check the top proc before restoring")

uninstall = body(source, "void Uninstall()")
if "RemoveAnsiShim(hwnd);" not in uninstall:
    fail("Uninstall must remove the ANSI shim")
if uninstall.index("RemoveAnsiShim(hwnd);") > uninstall.index("RemoveWindowSubclass(hwnd, &SubclassProc, kSubclassId)"):
    fail("Uninstall must remove the ANSI shim before RemoveWindowSubclass")

print("WebInput ANSI shim guard passed")
