#!/usr/bin/env python3
"""Generate runtime integrity pins from the authored Ultralight 1.4.0 manifest.

Release packaging still verifies every SDK file byte-for-byte against the JSON manifest.
The in-process runtime header intentionally does not hash-pin LGPL shared libraries that
end users must remain able to replace with interface-compatible builds.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_MANIFEST = ROOT / "runtime" / "ultralight-1.4.0.manifest.json"
DEFAULT_HEADER = ROOT / "src" / "PrismaUI" / "UltralightRuntimeManifest.g.h"

# AppCore and WebCore contain LGPL-covered code distributed as shared libraries. The
# official release candidate remains byte-verified by validate-ultralight-runtime.py,
# but the installed runtime must not refuse a user's interface-compatible replacement.
RUNTIME_REPLACEABLE_SHARED_LIBRARIES = {
    "libs/AppCore.dll",
    "libs/WebCore.dll",
}


def render(manifest_path: Path) -> str:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("runtime") != "ultralight" or manifest.get("version") != "1.4.0":
        raise ValueError("manifest is not the pinned Ultralight 1.4.0 manifest")
    files = manifest.get("files")
    if not isinstance(files, dict) or not files:
        raise ValueError("manifest has no runtime files")

    rows = []
    for name in sorted(files):
        posix = Path(name).as_posix()
        if posix in RUNTIME_REPLACEABLE_SHARED_LIBRARIES:
            continue
        meta = files[name]
        digest = str(meta["sha256"]).lower()
        if len(digest) != 64 or any(c not in "0123456789abcdef" for c in digest):
            raise ValueError(f"bad sha256 for {name}: {meta.get('sha256')!r}")
        size = int(meta["size"])
        rows.append(f'    PinnedRuntimeFile{{L"{posix}", {size}ull, "{digest}"}},')

    body = "\n".join(rows)
    return (
        "// GENERATED from runtime/ultralight-1.4.0.manifest.json by "
        "scripts/gen-runtime-manifest-header.py -- do not edit.\n"
        "// Release packaging pins all SDK bytes; AppCore.dll and WebCore.dll are intentionally\n"
        "// runtime-replaceable shared libraries and are therefore omitted from this integrity array.\n"
        "#pragma once\n"
        "\n"
        "#include <array>\n"
        "#include <cstdint>\n"
        "#include <string_view>\n"
        "\n"
        "namespace PrismaUI::WebRuntimeUltralight {\n"
        "    struct PinnedRuntimeFile {\n"
        "        std::wstring_view relative;\n"
        "        std::uint64_t size;\n"
        "        std::string_view sha256;\n"
        "    };\n"
        "\n"
        "    inline constexpr std::array kPinnedRuntimeFiles{\n"
        f"{body}\n"
        "    };\n"
        "}\n"
    )


def main() -> int:
    manifest_path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_MANIFEST
    header_path = Path(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_HEADER
    header_path.write_text(render(manifest_path), encoding="utf-8", newline="\n")
    print(f"wrote {header_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
