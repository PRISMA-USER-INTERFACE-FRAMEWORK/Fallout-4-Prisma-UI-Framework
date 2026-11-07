#!/usr/bin/env python3
"""Pin the Ultralight HLSL to its upstream AppCore 1.4 source, at two levels.

The accelerated GPUDriver compiles its shaders at runtime from raw-string literals in
src/GPU/UltralightShaders.h (no runtime file IO). The same stock AppCore 1.4 shaders are also vendored
under assets/shaders/ultralight-1.4/ for LGPL-2.1 source availability, and NOTICE.md records the exact
upstream Git blob SHA-1 of each. Two independent things can drift:

  1. Upstream identity -- the vendored .hlsl silently diverges from the AppCore 1.4 source. Caught by
     recomputing each file's Git blob SHA-1 and comparing to the SHA NOTICE.md claims. (Editing both
     the file and its NOTICE entry together defeats this, but that is now a deliberate, reviewable act
     of re-pinning to a new upstream revision, not an accident.)
  2. Embedded/vendored sync -- the raw string in UltralightShaders.h diverges from the .hlsl. Caught by
     comparing them content-equivalently (line endings, trailing whitespace, and the raw-string's outer
     blank lines normalized -- NOT a byte-for-byte comparison, hence "content-equivalent").
"""
import hashlib
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src" / "GPU" / "UltralightShaders.h"
SHADERS = ROOT / "assets" / "shaders" / "ultralight-1.4"
NOTICE = SHADERS / "NOTICE.md"

# Embedded symbol in UltralightShaders.h -> vendored .hlsl (relative to SHADERS) that is its source.
MAPPING = {
    "kFillPathVS": "vs/v2f_c4f_t2f.hlsl",
    "kFillPathPS": "ps/fill_path.hlsl",
    "kFillVS": "vs/v2f_c4f_t2f_t2f_d28f.hlsl",
    "kFillPS": "ps/fill.hlsl",
}

BLOCK = re.compile(r'inline\s+constexpr\s+char\s+(\w+)\s*\[\]\s*=\s*((?:R"ULHLSL\(.*?\)ULHLSL"\s*)+);',
                   re.S)
RAW_FRAGMENT = re.compile(r'R"ULHLSL\((.*?)\)ULHLSL"', re.S)
INCLUDE = re.compile(r'^#include "([^"]+)"$', re.M)
NOTICE_SHA = re.compile(r'`((?:vs|ps)/[^`]+\.hlsl)`\s*:\s*`([0-9a-f]{40})`')


def expanded(path: Path) -> str:
    return INCLUDE.sub(lambda m: expanded(path.parent / m.group(1)), path.read_text(encoding="utf-8", errors="replace"))


def normalize(text: str) -> str:
    """Meaningful content only: unify line endings, drop trailing whitespace per line, strip
    leading/trailing blank lines (the raw-string literal has an outer newline the file does not)."""
    lines = [ln.rstrip() for ln in text.replace("\r\n", "\n").replace("\r", "\n").split("\n")]
    while lines and not lines[0]:
        lines.pop(0)
    while lines and not lines[-1]:
        lines.pop()
    return "\n".join(lines)


def git_blob_sha1(path: Path) -> str:
    """Reproduce `git hash-object`: sha1("blob <len>\\0" + LF-normalized content). LF-normalizing
    matches how Git stores the file, so the SHA is stable regardless of the working-tree line endings."""
    data = path.read_bytes().replace(b"\r\n", b"\n")
    header = b"blob %d\x00" % len(data)
    return hashlib.sha1(header + data).hexdigest()


problems = []

for required in (HEADER, NOTICE):
    if not required.exists():
        print(f"FAIL: {required.relative_to(ROOT)} is missing")
        raise SystemExit(1)

embedded = {
    symbol: "".join(RAW_FRAGMENT.findall(initializer))
    for symbol, initializer in BLOCK.findall(expanded(HEADER))
}
upstream = dict(NOTICE_SHA.findall(NOTICE.read_text(encoding="utf-8", errors="replace")))

for symbol, rel in MAPPING.items():
    hlsl = SHADERS / rel
    if not hlsl.exists():
        problems.append(f"vendored {rel} is missing; it is the canonical source for {symbol}")
        continue

    # 1. Upstream identity: vendored file must hash to the SHA NOTICE.md pins.
    if rel not in upstream:
        problems.append(f"NOTICE.md records no upstream Git blob SHA for {rel}")
    else:
        actual = git_blob_sha1(hlsl)
        if actual != upstream[rel]:
            problems.append(
                f"{rel} has diverged from its pinned AppCore 1.4 upstream: NOTICE.md says "
                f"{upstream[rel]}, file hashes to {actual}. Re-vendor from upstream, or update NOTICE.md "
                f"deliberately if intentionally re-pinning.")

    # 2. Embedded/vendored sync: raw string must be content-equivalent to the .hlsl.
    if symbol not in embedded:
        problems.append(f"{HEADER.name} no longer defines {symbol} (expected source {rel})")
    elif normalize(embedded[symbol]) != normalize(hlsl.read_text(encoding="utf-8", errors="replace")):
        problems.append(
            f"{symbol} in {HEADER.name} has drifted from {rel} -- edit the .hlsl (the source of truth) "
            f"and copy it verbatim into the raw-string literal so both stay in sync")

# No embedded shader may skip the checks above.
for symbol in embedded:
    if symbol not in MAPPING:
        problems.append(
            f"{HEADER.name} defines {symbol} with no vendored .hlsl mapping in this guard -- add it to "
            f"assets/shaders/ultralight-1.4/, record its upstream SHA in NOTICE.md, and add it to MAPPING")

if problems:
    print("FAIL: Ultralight shader sources are not correctly pinned:")
    for p in problems:
        print(f"  {p}")
    raise SystemExit(1)

print(f"ultralight_shader_source_guard ok ({len(MAPPING)} shaders: upstream-pinned + embedded in sync)")
