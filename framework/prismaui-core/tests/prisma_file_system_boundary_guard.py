from pathlib import Path


backend = Path("src/PrismaUI/UltralightBackend.cpp").read_text(encoding="utf-8")
header = Path("src/PrismaUI/PrismaFileSystem.h").read_text(encoding="utf-8")
source = Path("src/PrismaUI/PrismaFileSystem.cpp").read_text(encoding="utf-8")
xmake = Path("xmake.lua").read_text(encoding="utf-8")

assert "class PrismaFileSystem final" in header
for symbol in ("FileExists", "GetFileMimeType", "GetFileCharset", "OpenFile", "Resolve"):
    assert f"PrismaFileSystem::{symbol}" in source
for symbol in ("MimeType", "ResolveBelow", "InjectContentSecurityPolicyIntoHtml", "InjectInspectorContentSecurityPolicyIntoHtml"):
    assert symbol in source
assert "class PrismaFileSystem final" not in backend
assert "src/PrismaUI/PrismaFileSystem.cpp" in xmake
assert len(header.splitlines()) < 500
assert len(source.splitlines()) < 500

print("Prisma file-system boundary guard passed")
