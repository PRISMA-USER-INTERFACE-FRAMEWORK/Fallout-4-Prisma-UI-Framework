import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODERN = (ROOT / "src" / "PrismaUI_F4_Modern_API.h").read_text(encoding="utf-8")
LEGACY = (ROOT / "src" / "PrismaUI_F4_API.h").read_text(encoding="utf-8")
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
VR = (ROOT / "src" / "PrismaUI_F4VR_API.h").read_text(encoding="utf-8")
VR_MAIN = (ROOT / "src" / "VR" / "VRMain.cpp").read_text(encoding="utf-8")

assert "class IVPrismaUI1" in LEGACY
assert "class IVPrismaUI12" in LEGACY
assert "class IVPrismaUI1" not in MODERN
assert "class IVPrismaUI12" not in MODERN
for version, token in ((9, 136), (10, 137), (11, 138), (12, 139)):
    assert re.search(rf"V{version}\s*=\s*(?:{token}|0x{token:X})", LEGACY)

for table in ("CoreAPI", "ControllerAPI", "GameThreadAPI"):
    block = re.search(rf"struct {table}\s*\{{(.*?)\n\s*\}};", MODERN, re.DOTALL)
    assert block, f"missing {table}"
    assert re.search(r"uint32_t structSize\s*;", block.group(1))
    assert re.search(r"uint32_t apiVersion\s*;", block.group(1))

assert "enum class ApiFeature" in MODERN
assert "PrismaUI_F4_GetAPI" in MODERN
assert "enum class LegacyInterfaceVersion" not in MODERN
assert "RequestPluginAPI" not in MODERN
for version in range(1, 11):
    assert f"InterfaceVersion::V{version}" in MAIN
assert "PrismaUIInterfaceV11" in MAIN
assert "PrismaUIInterfaceV12" in MAIN
assert re.search(r"default:\s*\n\s*logger::info\(\"RequestPluginAPI: unsupported interface version\"\);\s*\n\s*return nullptr;", MAIN)
assert re.search(r"RequestPluginAPI\(.*?default:.*?return nullptr;", VR_MAIN, re.DOTALL)
assert re.search(
    r"PrismaUI_F4_GetAPI\(.*?return PrismaUI::FlatAPI::GetVR\(feature, version, table, tableSize\);",
    VR_MAIN,
    re.DOTALL,
)
core = re.search(r"struct CoreAPI\s*\{(.*?)\n\s*\};", MODERN, re.DOTALL).group(1)
controller = re.search(r"struct ControllerAPI\s*\{(.*?)\n\s*\};", MODERN, re.DOTALL).group(1)
game_thread = re.search(r"struct GameThreadAPI\s*\{(.*?)\n\s*\};", MODERN, re.DOTALL).group(1)
assert "Controller" not in core
assert "Controller" not in game_thread
assert "BindControllerAction" in controller
assert "SetNativeGamepad" in controller
assert "SetNativeGamepad" not in LEGACY.split("class IVPrismaUI9 : public IVPrismaUI8", 1)[1].split(
    "class IVPrismaUI10 : public IVPrismaUI9", 1)[0]
assert "DispatchToGameThread" in game_thread
assert "//" not in MODERN
FLAT = (ROOT / "src" / "API" / "FlatAPI.cpp").read_text(encoding="utf-8")
assert "reinterpret_cast" not in FLAT
assert "RegisterConsoleCallbackFlat" in FLAT
assert "regions[i].x" in FLAT and "regions[i].height" in FLAT
assert "ControllerVR" in FLAT
assert "__builtin_offsetof(Flat::ControllerAPI, BindControllerFocusEntry)" in FLAT
assert "CopyTable(ControllerVR" in FLAT
assert "//" not in FLAT
assert 'extern "C" __declspec(dllexport) bool PrismaUI_F4_GetAPI' in MAIN
assert '#include "PrismaUI_F4_API.h"' in VR
assert "PRISMA_UI_VR_API::IVPrismaUIVR1" not in MODERN

for path in ROOT.parents[1].rglob("PrismaUI_F4_API.h"):
    if path == ROOT / "src" / "PrismaUI_F4_API.h":
        continue
    assert path.read_text(encoding="utf-8") == LEGACY, f"diverged vendored header: {path}"

assert re.search(r"enum class ApiFeature[^}]*Core\s*=\s*1[^}]*Controller\s*=\s*2[^}]*GameThread\s*=\s*3", MODERN, re.DOTALL)
assert re.search(r"Meta\s*=\s*4", MODERN)
assert re.search(r"View\s*=\s*5", MODERN)
assert re.search(r"Interop\s*=\s*6", MODERN)
assert re.search(r"Localization\s*=\s*7", MODERN)
assert re.search(r"Render\s*=\s*8", MODERN)
assert re.search(r"Input\s*=\s*9", MODERN)
assert re.search(r"Menu\s*=\s*10", MODERN)
assert re.search(r"Callbacks\s*=\s*11", MODERN)
assert re.search(r"CallbacksApiVersion\s*=\s*1", MODERN)
for table in ("MetaAPI", "ViewAPI", "InteropAPI", "LocalizationAPI", "RenderAPI", "InputAPI", "MenuAPI", "CallbacksAPI"):
    assert re.search(rf"struct {table}\s*\{{", MODERN), f"missing {table}"
    assert re.search(rf"is_trivially_copyable_v<{table}>", MODERN)
assert "PRISMA_FLAT_SLOT(CoreAPI, SetViewOffscreenBackground, 41)" in MODERN
assert "PRISMA_FLAT_SLOT(ControllerAPI, SetNativeGamepad, 19)" in MODERN
assert "PRISMA_FLAT_SLOT(GameThreadAPI, BindGameThreadUIEvent, 2)" in MODERN
assert "template <ApiFeature Feature> struct FeatureTable;" in MODERN
assert "using FeatureTableT = typename FeatureTable<Feature>::type;" in MODERN
assert "inline bool Discover(uint32_t minimumVersion, FeatureTableT<Feature>& table)" in MODERN
assert "inline bool HasCapability(" in MODERN
assert "table.structSize < static_cast<uint32_t>(sizeof(table))" in MODERN
assert "PRISMA_FLAT_SLOT(CallbacksAPI, RegisterConsoleCallbackEx, 2)" in MODERN
assert "PluginAPI::VerifiedPrismaUIInterface* CallbackApi()" in FLAT
assert "MetaHasFeatureVR" in FLAT
assert "CopyTable(MetaVR" in FLAT
assert "table.apiVersion < minimumVersion" in MODERN
assert "SetViewCursorPolicy" in MODERN and "GetViewCursorPolicy" in MODERN
assert "RegisterTranslationsV4" in MODERN
assert "SupportedVersion" in FLAT
assert "CopyTable(Core, PRISMA_UI_FLAT_API::CoreApiVersion, table, tableSize)" in FLAT
assert "std::memset(bytes + copyBytes, 0, outputSize - copyBytes)" in FLAT
assert "PRISMA_UI_FLAT_API::MetaApiVersion" in FLAT

print("flat API contract guard passed")
