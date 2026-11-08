#!/usr/bin/env python3
"""No new by-name Prisma DLL lookups in framework code.

GetModuleHandleW(L"PrismaUI_F4.dll") is correct only for the flat build. In the VR build the
module is PrismaUI_F4VR.dll, so the lookup returns null, GetModuleFileNameW(nullptr, ...) then
quietly returns the EXE path, and every path derived from it points at the game root instead of
the plugin directory. src/Utils/ModulePath.h resolves the module from its own address instead.

Scope: the framework DLL's own sources -- the code that can actually reach ModulePath.h. Skipped,
with reasons, in SKIPPED_TREES below.
"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
PATTERN = re.compile(r'GetModuleHandleW\(L"Prisma')

# Trees whose code cannot use the helper, so a hit there is not the bug this guard is about.
# Empty since the CEF Host and subprocess targets were retired; every remaining target builds with
# src/ on its include path, so ModulePath.h is reachable everywhere.
SKIPPED_TREES = set()

# Individual files, each with the reason it is exempt.
ALLOWED = {
    # The consumer SDK headers, compiled into OTHER people's plugins. Probing for PrismaUI_F4.dll
    # and then PrismaUI_F4VR.dll is how a plugin FINDS the framework; it is not the framework
    # locating itself, and it must not depend on framework-internal headers.
    Path("PrismaUI_F4_API.h"),
    Path("PrismaUI_F4_Modern_API.h"),
    Path("PrismaUI_F4VR_API.h"),
}


def strip_comments(text: str) -> str:
    """Drop // and /* */ so the ban documented inside ModulePath.h is not read as a violation."""
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


hits = set()
for path in SRC.rglob("*"):
    if path.suffix not in {".cpp", ".h", ".hpp"}:
        continue
    rel = path.relative_to(SRC)
    if rel.parts and rel.parts[0] in SKIPPED_TREES:
        continue
    if PATTERN.search(strip_comments(path.read_text(encoding="utf-8", errors="replace"))):
        hits.add(rel)

unexpected = sorted(hits - ALLOWED)
missing = sorted(ALLOWED - hits)

if unexpected:
    print("FAIL: new by-name Prisma DLL lookup outside the canonical module-path helper:")
    for path in unexpected:
        print(f"  src/{path}")
    print("Use PrismaUI::Utils::ThisModule() / ThisModuleDir() / PluginIniPath() instead.")
    raise SystemExit(1)

if missing:
    print("FAIL: allowlist is stale; these were fixed, so remove them from ALLOWED:")
    for path in missing:
        print(f"  src/{path}")
    raise SystemExit(1)

print(f"module_path_guard ok ({len(hits)} allowlisted site(s) remain, 0 new)")
