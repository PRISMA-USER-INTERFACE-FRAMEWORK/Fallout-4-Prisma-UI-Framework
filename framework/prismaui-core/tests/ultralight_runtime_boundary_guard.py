import re
from pathlib import Path


backend = Path("src/PrismaUI/UltralightBackend.cpp").read_text(encoding="utf-8")
header = Path("src/PrismaUI/UltralightRuntime.h").read_text(encoding="utf-8")
source = Path("src/PrismaUI/UltralightRuntime.cpp").read_text(encoding="utf-8")
xmake = Path("xmake.lua").read_text(encoding="utf-8")

assert "class UltralightRuntime final" in header
for symbol in ("UltralightPackageRoot", "UltralightViewRoot", "UltralightRuntime::Load", "ValidateRuntimePackage",
               "UltralightDelayLoadHook", "__pfnDliNotifyHook2"):
    assert symbol in source
for symbol in ("UltralightRuntimeManifest.g.h", "ValidateRuntimePackage", "UltralightDelayLoadHook",
               "UltralightRuntimeState"):
    assert symbol not in backend
assert re.search(r"std::make_unique<PrismaFileSystem>\(\s*UltralightViewRoot\(\),", backend)
assert "src/PrismaUI/UltralightRuntime.cpp" in xmake
assert len(header.splitlines()) < 500
assert len(source.splitlines()) < 500

print("Ultralight runtime boundary guard passed")
