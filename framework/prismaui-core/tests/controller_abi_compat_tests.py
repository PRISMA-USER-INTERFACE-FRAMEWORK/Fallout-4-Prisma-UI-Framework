from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
header = (root / "src" / "PrismaUI_F4_API.h").read_text(encoding="utf-8")
modern = (root / "src" / "PrismaUI_F4_Modern_API.h").read_text(encoding="utf-8")
provider = (root / "src" / "main.cpp").read_text(encoding="utf-8")
api = (root / "src" / "API" / "API.h").read_text(encoding="utf-8")
checker = (root / "src" / "Utils" / "ConflictChecker.cpp").read_text(encoding="utf-8")

assert re.search(r"V9\s*=\s*136.*V10\s*=\s*137.*V11\s*=\s*138.*V12\s*=\s*139", header, re.DOTALL)
assert "PrismaUIInterfaceV13" not in header
assert "class IVPrismaUI13" not in header
assert "class IVPrismaUI14" not in header
assert "using LatestInterface = PRISMA_UI_API::IVPrismaUI12;" in api

v9 = header[header.index("class IVPrismaUI9 : public IVPrismaUI8") : header.index("class IVPrismaUI10 : public IVPrismaUI9")]
v12 = header[header.index("class IVPrismaUI12 : public IVPrismaUI11") : header.index("struct InterfaceVersionMap")]
for method in (
    "IsUsingGamepad",
    "GetControllerStyle",
    "SetControllerStyle",
    "NoteInputDevice",
    "GetButtonPrompt",
    "GetGamepadButtonName",
    "SetViewOwnsEscape",
    "SetViewOffscreenBackground",
):
    assert method in v9, f"IVPrismaUI9 missing {method}"
assert "BindControllerAction" not in v9
assert "SetNativeGamepad" not in v9
assert "uint32_t buttonCode" not in v12
assert "const char* canonicalButton" in v12
assert "SetNativeGamepad" in modern
assert "bool SetNativeGamepad(PrismaView view, bool enabled) noexcept;" in api
assert "virtual bool SetNativeGamepad" not in api

for version, needle in (
    ("V1", "case PRISMA_UI_API::InterfaceVersion::V1:"),
    ("V8", "case PRISMA_UI_API::InterfaceVersion::V8:"),
    ("V9", "case PRISMA_UI_API::InterfaceVersion::V9:"),
    ("V10", "case PRISMA_UI_API::InterfaceVersion::V10:"),
    ("V11", "case PRISMA_UI_API::PrismaUIInterfaceV11:"),
    ("V12", "case PRISMA_UI_API::PrismaUIInterfaceV12:"),
):
    assert needle in provider, f"provider missing {version} case"

v12_case = provider.split("case PRISMA_UI_API::PrismaUIInterfaceV12:", 1)[1].split("default:", 1)[0]
assert "IVPrismaUI12" in v12_case and "nullptr" not in v12_case

default_case = provider.split("default:", 1)[1].split("}", 1)[0]
assert "return nullptr;" in default_case
assert "PrismaUIInterfaceV13" not in provider

assert "case PRISMA_UI_API::PrismaUIInterfaceV12:" in checker
assert "FormatInterfaceVersionLabel" in checker
assert "V9 (token 0x88)" in checker
assert "V12 (token 0x8B)" in checker

print("controller ABI compatibility guard passed")
