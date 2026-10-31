#!/usr/bin/env python3
"""Every framework ini key the code reads must be documented in the shipped ini.

A setting nobody can discover does not exist. [Perf] SkipIdleComposite gates a real rendering
optimisation and was readable by the code for a whole release without appearing in
release-assets/PrismaUI_F4.ini, so no player could ever have turned it on.

Checks two directions:
  * every [Section] Key read from the FRAMEWORK ini appears in the shipped file
  * every file that reads an ini at all is classified, so a new one cannot quietly read the
    framework ini without being covered here

The release seed must keep MeshBinding disabled until its active consumers have runtime acceptance.
"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
SHIPPED = ROOT / "release-assets" / "PrismaUI_F4.ini"

# Files that read PrismaUI_F4.ini itself -- the file players edit.
FRAMEWORK_INI_READERS = {
    "API/MeshBinding.cpp", "API/MeshBindingInternal.h", "PrismaUI/WebRuntimePresent.cpp",
    "PrismaUI/WebCompositor.cpp", "PrismaUI/ControllerGlyphs.cpp", "PrismaUI/DevToolsConfig.h",
    "PrismaUI/FreezeDiagnostics.cpp", "PrismaUI/NetworkAuditConfig.h", "PrismaUI/OffscreenFrames.cpp",
    "PrismaUI/PresentProfiler.cpp", "PrismaUI/UltralightBackend.cpp", "PrismaUI/MenuCursorDiagnostics.cpp",
    "main.cpp",
}

# Files that read a DIFFERENT ini, with which one, so they are not held to the shipped file.
OTHER_INI_READERS = {
    "PrismaUI/ModelPreview.cpp": "PrismaUI_ModelPreview.ini",
    "PrismaUI/LocalMapSpike.cpp": "PrismaUI_LocalMapSpike.ini",
    "PrismaUI/CameraSpike.cpp": "PrismaUI_CameraSpike.ini",
    "PrismaUI/ActorPreviewSpike.cpp": "PrismaUI_ActorPreviewSpike.ini",
    "PrismaUI/Translations.cpp": "the GAME's Fallout4.ini / Fallout4Custom.ini",
}

READ = re.compile(
    r'(?:GetPrivateProfile(?:Int|String)[AW]|ReadIniBool)\s*\(\s*L?"([^"]+)"\s*,\s*L?"([^"]+)"')


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


unclassified, wanted = [], set()
for path in sorted(SRC.rglob("*")):
    if path.suffix not in {".cpp", ".h", ".hpp", ".inl"}:
        continue
    rel = path.relative_to(SRC).as_posix()
    body = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
    hits = READ.findall(body)
    if not hits:
        continue
    if rel in OTHER_INI_READERS:
        continue
    if rel not in FRAMEWORK_INI_READERS:
        unclassified.append(rel)
        continue
    wanted.update(hits)

if unclassified:
    print("FAIL: these files read an ini but are not classified by this guard:")
    for rel in unclassified:
        print(f"  src/{rel}")
    print("Add it to FRAMEWORK_INI_READERS (and document its keys in the shipped ini),")
    print("or to OTHER_INI_READERS with the name of the ini it actually reads.")
    raise SystemExit(1)

if not SHIPPED.exists():
    print(f"FAIL: {SHIPPED.relative_to(ROOT)} is missing")
    raise SystemExit(1)

shipped, section = set(), None
for line in SHIPPED.read_text(encoding="utf-8", errors="replace").splitlines():
    s = line.strip()
    if s.startswith("[") and s.endswith("]"):
        section = s[1:-1]
    elif s and not s.startswith((";", "#")) and "=" in s and section:
        shipped.add((section, s.split("=", 1)[0].strip()))

missing = sorted(wanted - shipped)
if missing:
    print("FAIL: framework ini keys read by the code but absent from release-assets/PrismaUI_F4.ini:")
    for sec, key in missing:
        print(f"  [{sec}] {key}")
    print("Players cannot discover a setting that is not in the shipped file. Document it there.")
    raise SystemExit(1)

stale = sorted(shipped - wanted)
if stale:
    print("FAIL: shipped ini documents keys nothing reads:")
    for sec, key in stale:
        print(f"  [{sec}] {key}")
    print("Remove them, or fix the reader this guard failed to see.")
    raise SystemExit(1)

if ("Debug", "bMeshBinding") not in shipped or not re.search(
    r"^\s*bMeshBinding\s*=\s*0\s*(?:[;#].*)?$", SHIPPED.read_text(encoding="utf-8", errors="replace"), re.M
):
    print("FAIL: release-assets/PrismaUI_F4.ini must ship [Debug] bMeshBinding=0")
    raise SystemExit(1)

print(f"ini_documentation_guard ok ({len(wanted)} framework key(s), all documented)")
