#!/usr/bin/env python3
"""Build a case-alias mirror of a header tree.

Replaces two bash loops that spawned basename/tr/printf per file. On the release runner those
cost 92s for the Windows SDK (1935 aliases) and 147s for CommonLibF4VR (1666); this does the same
work in about a second, because the process-per-file was the entire expense.

Why any of it is needed: headers get included by a spelling that does not match the file on disk --
CEF's `#include <Softpub.h>` against xwin's lowercase splat, CommonLibF4VR's
`#include <F4SE/interfaces.h>` against its own Interfaces.h. MSVC on NTFS never notices; a Linux
cross-build stops dead. Symlinks only: nothing is copied and no source tree is modified.

  stage-case-shim.py <shim-dir> --flat <dir> [<dir>...]     aliases at the shim root (SDK style)
  stage-case-shim.py <shim-dir> --tree <root>               mirrors the tree (CommonLibF4VR style)
  ... --require <relative/path.h>                           assert an alias exists, else fail
"""
import argparse
import os
import shutil
import sys
from pathlib import Path


def variants(name: str) -> set[str]:
    """The spellings a header might be included by: as-is, lowercased, capitalised first letter."""
    return {name, name.lower(), name[:1].upper() + name[1:]}


def link(target: Path, src: Path) -> int:
    if target.exists() or target.is_symlink():
        return 0
    target.parent.mkdir(parents=True, exist_ok=True)
    target.symlink_to(src)
    return 1


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("shim")
    ap.add_argument("--flat", nargs="*", default=[])
    ap.add_argument("--tree", nargs="*", default=[])
    ap.add_argument("--require", nargs="*", default=[])
    a = ap.parse_args()

    shim = Path(a.shim)
    if shim.exists():
        shutil.rmtree(shim)
    shim.mkdir(parents=True)

    made = 0
    # Flat: every header collapses into the shim root under each spelling. Correct for the Windows
    # SDK, whose headers are included unqualified.
    for d in a.flat:
        p = Path(d)
        if not p.is_dir():
            continue
        for f in p.glob("*.h"):
            for v in variants(f.name):
                made += link(shim / v, f.resolve())

    # Tree: mirror the directory layout, since these are included path-qualified (F4SE/..., RE/...).
    for root in a.tree:
        r = Path(root)
        if not r.is_dir():
            print(f"include tree not found: {root}", file=sys.stderr)
            return 1
        for dirpath, _, files in os.walk(r):
            rel = Path(dirpath).relative_to(r)
            rels = {rel, Path(str(rel).lower())} if str(rel) != "." else {Path(".")}
            for f in files:
                if not f.endswith(".h"):
                    continue
                src = (Path(dirpath) / f).resolve()
                for rd in rels:
                    for v in variants(f):
                        made += link(shim / rd / v, src)

    print(f"case-alias symlinks: {made}")

    # Assert the specific aliases past failures needed, so a future SDK or submodule bump fails
    # here with the reason rather than deep inside a 200-TU compile.
    for req in a.require:
        if not (shim / req).exists():
            print(f"required alias missing: {shim / req}", file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
