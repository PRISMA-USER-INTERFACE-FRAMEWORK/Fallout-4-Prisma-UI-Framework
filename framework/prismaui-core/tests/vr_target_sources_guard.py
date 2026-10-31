#!/usr/bin/env python3
"""The VR target enumerates its sources; the flat target globs. Keep them in sync.

PrismaUI_F4 uses `add_files("src/**.cpp")`, so a new translation unit is picked up for free.
PrismaUI_F4VR lists each file explicitly and deliberately EXCLUDES the modules that hook
1.10.163-exact OG addresses or OG vtable RVAs. Three ways that goes wrong, all of which surface
only as unresolved externals on a runner:

  1. A file that IS in the VR list gets split, and the new half is not added to the list.
  2. VR-compiled code calls into an EXCLUDED module. Its .cpp is not linked, so the call needs a
     stub in src/VR/VRUnportedStubs.cpp. This is how ModelPreview::HasPendingWork and
     TextureOverlay::HasPendingWork broke the VR link.
  3. A shared source includes a module whose .cpp exists but is intentionally header-only for VR;
     that is valid only when the .cpp has no definitions or the header provides explicit VR inline
     stubs for every call the shared source still makes.

Everything here is evaluated against what the VR preprocessor actually sees: `#ifndef
PRISMAUI_FO4VR` blocks are stripped first, so a call that is already compiled out is not
demanded to have a stub.
"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
XMAKE = ROOT / "xmake.lua"
SRC = ROOT / "src"
STUBS = SRC / "VR" / "VRUnportedStubs.cpp"

# Named in the xmake comment as deliberately absent: OG-address-bound, or a flat-only path with a
# VR counterpart. Their headers may still be included; their symbols must be stubbed.
EXCLUDED = {
    "ModelPreview", "ModelPreviewInterface3D", "LocalMapSpike", "CameraSpike", "ActorPreviewSpike",
    "TextureOverlay",
    "VanillaUISuppressor", "WebCompositor", "GameThreadDispatcher", "VirtualPointer",
    "MenuCursorDiagnostics",
}
IFNDEF_VR = re.compile(r'^[ \t]*#[ \t]*ifndef[ \t]+PRISMAUI_FO4VR\b.*?^[ \t]*#[ \t]*endif[^\n]*\n',
                       re.S | re.M)
VR_DEFINED_BRANCH = re.compile(
    r'^[ \t]*#[ \t]*if[ \t]+defined\([ \t]*PRISMAUI_FO4VR[ \t]*\)[^\n]*\n(.*?)'
    r'^[ \t]*#[ \t]*(?:else|endif)\b',
    re.S | re.M,
)


def vr_visible(text: str) -> str:
    """What the VR compile sees: drop every #ifndef PRISMAUI_FO4VR block."""
    prev = None
    while prev != text:                       # repeat so nested/sequential blocks all go
        prev = text
        text = IFNDEF_VR.sub("", text)
    return text


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def translation_unit_has_definitions(path: Path) -> bool:
    """True when a .cpp contains anything substantive beyond includes/comments.

    A compatibility .cpp that only includes its header exists for flat source-glob/history reasons but
    creates no link dependency. Treating that as a required VR translation unit is a false positive.
    """
    body = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
    body = re.sub(r'^\s*#include[^\n]*\n', '', body, flags=re.M)
    return bool(body.strip())


def header_vr_inline_stubs(header: Path, functions: set[str]) -> bool:
    """Whether an explicit `#if defined(PRISMAUI_FO4VR)` branch inline-defines every called function."""
    if not functions or not header.exists():
        return False
    text = strip_comments(header.read_text(encoding="utf-8", errors="replace"))
    branches = VR_DEFINED_BRANCH.findall(text)
    if not branches:
        return False
    available = "\n".join(branches)
    return all(re.search(rf'\binline\s+[^;{{}}]*\b{re.escape(fn)}\s*\([^;{{}}]*\)\s*{{', available)
               for fn in functions)


def header_inline_stub(header: Path, fn: str) -> bool:
    if not header.exists():
        return False
    text = strip_comments(vr_visible(header.read_text(encoding="utf-8", errors="replace")))
    return re.search(rf'\binline\s+[^;{{}}]*\b{re.escape(fn)}\s*\([^;{{}}]*\)\s*{{', text) is not None


text = XMAKE.read_text(encoding="utf-8", errors="replace")
start = text.find('target("PrismaUI_F4VR")')
if start < 0:
    print("FAIL: PrismaUI_F4VR target not found in xmake.lua")
    raise SystemExit(1)
tail = text[start + 10:]
block = text[start:start + 10 + tail.find('target(')] if 'target(' in tail else text[start:]

listed = set(re.findall(r'"(src/PrismaUI/[A-Za-z0-9_]+\.cpp)"', block))
if not listed:
    print("FAIL: the VR target lists no src/PrismaUI sources; the guard cannot be right.")
    raise SystemExit(1)

if "src/PrismaUI/UltralightDispatchQueue.cpp" not in listed:
    problems = ["src/PrismaUI/UltralightDispatchQueue.cpp is required by UltralightBackend.h"]
else:
    problems = []

if not STUBS.exists():
    print("FAIL: src/VR/VRUnportedStubs.cpp is missing; the VR target has no stand-in for the "
          "excluded modules.")
    raise SystemExit(1)
stub_body = STUBS.read_text(encoding="utf-8", errors="replace")


def stubs_for(module: str) -> str:
    """The body of `namespace PrismaUI::<module> { ... }` in the stub file.

    Namespace-scoped on purpose: a name-only search across the whole file would let
    TextureOverlay's stub satisfy a missing ModelPreview one, since both declare HasPendingWork.
    """
    m = re.search(rf'namespace\s+PrismaUI::{re.escape(module)}\b', stub_body)
    if not m:
        return ""
    i = stub_body.find("{", m.end())
    if i < 0:
        return ""
    depth, j = 0, i
    while j < len(stub_body):
        if stub_body[j] == "{":
            depth += 1
        elif stub_body[j] == "}":
            depth -= 1
            if depth == 0:
                return stub_body[i:j]
        j += 1
    return stub_body[i:]

for rel in sorted(listed):
    path = ROOT / rel
    if not path.exists():
        problems.append(f"{rel} is listed for VR but does not exist")
        continue

    body = strip_comments(vr_visible((ROOT / rel).read_text(encoding="utf-8", errors="replace")))

    for inc in sorted(set(re.findall(r'#include\s+"([A-Za-z0-9_]+)\.h"', body))):
        implementation = SRC / "PrismaUI" / f"{inc}.cpp"
        if not implementation.exists():
            continue                                   # header-only module, nothing to link

        called = set(re.findall(rf'\b{re.escape(inc)}::([A-Za-z_]\w*)\s*\(', body))

        if inc in EXCLUDED:
            # Not linked for VR: every call the VR build still makes needs a stub.
            available = stubs_for(inc)
            header = SRC / "PrismaUI" / f"{inc}.h"
            for fn in sorted(called):
                if not re.search(rf'\b{re.escape(fn)}\s*\(', available) and not header_inline_stub(header, fn):
                    problems.append(
                        f"{rel} calls {inc}::{fn}() and is in the VR target, but "
                        f"src/PrismaUI/{inc}.cpp is not -- add a stub to src/VR/VRUnportedStubs.cpp")
            continue

        if f"src/PrismaUI/{inc}.cpp" not in listed:
            # Some shared modules intentionally keep a compatibility .cpp that has no definitions;
            # there is no object-file dependency in that case. Others expose explicit VR inline stubs
            # in their header so the shared caller has no unresolved external even though flat has a .cpp.
            if not translation_unit_has_definitions(implementation):
                continue
            if header_vr_inline_stubs(SRC / "PrismaUI" / f"{inc}.h", called):
                continue
            problems.append(
                f"{rel} includes {inc}.h, and src/PrismaUI/{inc}.cpp exists, but the VR target "
                f"does not list it -> unresolved externals in the VR link")

if problems:
    print("FAIL: PrismaUI_F4VR would not link:")
    for p in problems:
        print(f"  {p}")
    print("Add the file to the VR target's add_files(), provide explicit VR inline stubs, make the module "
          "truly header-only, or compile the call out with #ifndef PRISMAUI_FO4VR.")
    raise SystemExit(1)

print(f"vr_target_sources_guard ok ({len(listed)} VR-listed PrismaUI sources, "
      f"all deps linked, header-only, or stubbed)")
