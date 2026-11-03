from pathlib import Path

root = Path(__file__).resolve().parents[1]
vr_header = (root / "src" / "PrismaUI_F4VR_API.h").read_text(encoding="utf-8")

start = vr_header.index("inline void* RequestPluginVRAPI(")
body = vr_header[start:vr_header.index("template <class Interface>", start)]

assert 'GetModuleHandleW(L"PrismaUI_F4VR.dll")' in body
assert "GetPrismaProviderModule" not in body
assert "PrismaUI_F4.dll" not in body

print("VR provider resolver guard passed")
