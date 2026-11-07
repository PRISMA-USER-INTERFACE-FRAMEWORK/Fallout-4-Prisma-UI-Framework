"""The version a binary REPORTS must be the version it IS.

`F4SE_PLUGIN_VERSION` is what F4SE logs at load and what every bug report quotes. It used to carry
a hand-typed literal, and the two entry points had drifted to different wrong answers:

    src/main.cpp      v.PluginVersion({ 2, 2, 4, 0 })     -- last true at 2.2.4
    src/VR/VRMain.cpp version.PluginVersion({1, 0, 0, 0}) -- never true

On 2026-08-22 that cost a live debugging session: a correctly installed 2.0.8 announced itself as
`PrismaUI_F4 v2.2.4.0`, the log was read as proof the wrong build was deployed, and the install was
only confirmed correct by hashing the DLL against the published release zip.

Both now come from PRISMA_VERSION in xmake.lua, which is also what the release workflow checks the
tag against -- so tag, package name, and the number in the log cannot disagree.
"""

import re
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
xmake = (root / "xmake.lua").read_text(encoding="utf-8")
package_script = (root / "scripts" / "ci-package-release.py").read_text(encoding="utf-8")

failures = []

# -- the defines exist, are derived, and are visible to every target ------------------------------
version = re.search(r'^local PRISMA_VERSION = "(\d+\.\d+\.\d+)"', xmake, re.M)
if not version:
    failures.append("PRISMA_VERSION is missing or not x.y.z")

if "PRISMA_VERSION:match" not in xmake:
    failures.append("the version defines are not derived from PRISMA_VERSION")

define = re.search(r'^add_defines\("PRISMA_VERSION_MAJOR=', xmake, re.M)
if not define:
    failures.append(
        "the PRISMA_VERSION_* defines are not at ROOT scope in xmake.lua. Inside a target they "
        "reach that target only -- PrismaUI_F4 and PrismaUI_F4VR are separate targets and both "
        "report a version, which is how they drifted apart in the first place."
    )

if "configured = configured_version()" not in package_script or "if version != configured:" not in package_script:
    failures.append("release packager does not reject a package version that differs from PRISMA_VERSION")
if "package version {version} does not match compiled PRISMA_VERSION {configured}" not in package_script:
    failures.append("release packager lost the explicit package/binary version mismatch failure")

# -- no entry point may hard-code the version it reports ------------------------------------------
for rel in ("src/main.cpp", "src/VR/VRMain.cpp"):
    path = root / rel
    if not path.is_file():
        failures.append(f"{rel} is missing")
        continue
    text = path.read_text(encoding="utf-8")
    code = "\n".join(l for l in text.splitlines() if not l.strip().startswith("//"))

    literal = re.search(r"PluginVersion\(\s*\{\s*\d+\s*,", code)
    if literal:
        failures.append(
            f"{rel} hard-codes the version it reports: {literal.group(0).strip()} -- use the "
            "PRISMA_VERSION_* macros so the log matches the release"
        )
    if not re.search(r"PluginVersion\(\s*\{\s*PRISMA_VERSION_MAJOR", code):
        failures.append(f"{rel} does not report PRISMA_VERSION_MAJOR/MINOR/PATCH")

    # The legacy F4SE PluginInfo::version field is what shows in F4SE's plugin listing and must not
    # drift either (it was a literal 224 on flat and packed 1.0.0.0 on VR).
    reported = re.search(r"->version\s*=\s*([^;]+);", code)
    if not reported:
        failures.append(f"{rel} does not set the legacy PluginInfo::version")
    elif "PRISMA_VERSION_MAJOR" not in reported.group(1):
        failures.append(
            f"{rel} hard-codes the legacy PluginInfo::version ({reported.group(1).strip()}) -- derive "
            "it from PRISMA_VERSION_* too"
        )

# -- a build without the defines must fail loudly, not silently report 0.0.0 ----------------------
main_text = (root / "src" / "main.cpp").read_text(encoding="utf-8")
if "#    error" not in main_text and "#error" not in main_text:
    failures.append(
        "src/main.cpp does not #error when PRISMA_VERSION_* is undefined; an undefined macro in a "
        "constant expression is 0, so the binary would quietly report 0.0.0.0"
    )

if failures:
    print("plugin version guard failed:\n")
    for f in failures:
        print(f"  - {f}")
    sys.exit(1)

print(f"plugin version guard passed (binaries report PRISMA_VERSION {version.group(1)})")
