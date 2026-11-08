import hashlib
import re
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / "scripts"))

header = (root / "src" / "PrismaUI_F4_API.h").read_text(encoding="utf-8")
legacy, separator, newer = header.partition("using GameThreadTaskCallback")
assert separator, "V11 declarations must remain after the frozen V1-V10 API"

maps = []
for version in range(1, 11):
    match = re.search(
        rf"template <>\s*struct InterfaceVersionMap<IVPrismaUI{version}>\s*{{.*?}};",
        header,
        flags=re.DOTALL,
    )
    assert match, f"missing V{version} interface-version mapping"
    maps.append(match.group())

frozen = re.sub(r"/\*.*?\*/|//[^\n]*|\s+", "", legacy + "\n" + "\n".join(maps), flags=re.DOTALL)
digest = hashlib.sha256(frozen.encode()).hexdigest()

assert digest == "5e481e0be677f412ac49f267d38de6e36b63c7278d5d7d825098ed9d04dc7f4d"
assert "class IVPrismaUI11 : public IVPrismaUI10" in newer
assert "class IVPrismaUI12 : public IVPrismaUI11" in newer


def ordered_virtuals(interface: str, parent: str) -> list[str]:
    match = re.search(
        rf"class {interface}\s*:\s*public {parent}\s*{{(.*?)\n\s*}};",
        header,
        flags=re.DOTALL,
    )
    assert match, f"missing {interface} declaration"
    return [
        re.sub(r"\s+", " ", signature).strip()
        for signature in re.findall(r"virtual\s+.*?=\s*0\s*;", match.group(1), flags=re.DOTALL)
    ]


assert ordered_virtuals("IVPrismaUI11", "IVPrismaUI10") == [
    "virtual bool DispatchToGameThread(GameThreadTaskCallback callback, void* userdata) noexcept = 0;",
    "virtual bool IsGameThread() noexcept = 0;",
    "virtual bool BindGameThreadUIEvent(PrismaView view, const char* functionName, GameThreadUIEventCallback callback, void* userdata) noexcept = 0;",
]
assert ordered_virtuals("IVPrismaUI12", "IVPrismaUI11") == [
    "virtual bool BindControllerAction(PrismaView view, const char* canonicalButton, const char* action) noexcept = 0;",
    "virtual bool UnbindControllerAction(PrismaView view, const char* canonicalButton) noexcept = 0;",
    "virtual void ClearControllerActions(PrismaView view) noexcept = 0;",
    "virtual ControllerActionBridgeState GetControllerActionBridgeState(PrismaView view) noexcept = 0;",
    "virtual bool BindControllerFocusEntry(PrismaView view, const char* canonicalButton, GameThreadTaskCallback callback, void* userdata) noexcept = 0;",
    "virtual bool UnbindControllerFocusEntry(PrismaView view, const char* canonicalButton) noexcept = 0;",
]

for version in (11, 12):
    match = re.search(
        rf"template <>\s*struct InterfaceVersionMap<IVPrismaUI{version}>\s*{{(.*?)}};",
        header,
        flags=re.DOTALL,
    )
    assert match, f"missing V{version} interface-version mapping"
    normalized = re.sub(r"\s+", " ", match.group(1)).strip()
    assert f"InterfaceVersion::V{version}" in normalized, f"V{version} maps to the wrong interface version"

print("public API ABI guard passed")
