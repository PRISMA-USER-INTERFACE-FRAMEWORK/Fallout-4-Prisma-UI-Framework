import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
# controller_action_source_tests.py also pins inline VR branches in these files; update it when one moves.
BASELINE = {
    "src/API/ControllerActions.cpp": 1,
    "src/Engine/ControllerMenuKey.h": 1,
    "src/Engine/EngineControllerInput.h": 1,
    "src/Engine/EngineInterface3D.h": 1,
    "src/Engine/EngineTextureLayoutRuntime.h": 1,
    "src/GPU/FalloutD3DContext.cpp": 2,
    "src/PrismaUI/ControllerGlyphs.cpp": 2,
    "src/PrismaUI/Translations.cpp": 1,
    "src/PrismaUI/WebInput.cpp": 2,
    "src/PrismaUI/WebRuntimeFocus.cpp": 1,
    "src/PrismaUI/WebRuntimeInspector.cpp": 1,
    "src/PrismaUI/WebRuntimeLifecycle.cpp": 5,
    "src/PrismaUI/WebRuntimeState.cpp": 1,
}
DIRECTIVE = re.compile(r"^\s*#\s*(?:if|ifdef|ifndef|elif)\b.*\bPRISMAUI_FO4VR\b")
INCLUDE_VR = re.compile(r'^\s*#\s*include\s*[<"](?:[^">]*/)?VR/', re.M)
XMAKE = (ROOT / "xmake.lua").read_text(encoding="utf-8")

flat = re.search(r'^target\("PrismaUI_F4"\).*?(?=^target\()', XMAKE, re.M | re.S).group(0)
code = [line.strip() for line in flat.splitlines() if not line.lstrip().startswith("--")]
assert 'add_files("src/**.cpp", "src/resource.rc")' in code
assert 'remove_files("src/VR/**.cpp")' in code
assert [line for line in code if "src/VR" in line] == ['remove_files("src/VR/**.cpp")']
assert not (SRC / "compat" / "vr").exists() and "compat/vr" not in XMAKE

found = {}
problems = []
for path in sorted(SRC.rglob("*")):
    if path.suffix not in (".h", ".cpp") or path.relative_to(SRC).parts[0] == "VR":
        continue
    text = path.read_text(encoding="utf-8")
    rel = path.relative_to(ROOT).as_posix()
    count = sum(1 for line in text.splitlines() if DIRECTIVE.search(line))
    if count:
        found[rel] = count
    if INCLUDE_VR.search(text):
        problems.append(f"{rel} includes a src/VR header")

for rel, count in sorted(found.items()):
    if rel not in BASELINE:
        problems.append(f"{rel}: {count} PRISMAUI_FO4VR directive line(s), file is not allowlisted")
    elif count > BASELINE[rel]:
        problems.append(f"{rel}: {count} PRISMAUI_FO4VR directive lines, baseline {BASELINE[rel]}")
if problems:
    raise SystemExit("VR source ownership broken:\n  " + "\n  ".join(problems))

for rel, allowed in sorted(BASELINE.items()):
    count = found.get(rel, 0)
    if count < allowed:
        print(f"lower the baseline: {rel} {allowed} -> {count}")

print(f"VR source ownership guard passed ({len(found)} files, {sum(found.values())} directive lines)")
