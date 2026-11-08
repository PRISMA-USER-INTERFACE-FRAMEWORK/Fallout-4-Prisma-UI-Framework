import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

bridge = (ROOT / "src/PrismaUI/PapyrusBridge.cpp").read_text(encoding="utf-8")
assert "readClipboard: function()" in bridge, "window.prisma must expose readClipboard for the OSK Paste key"
assert 'op == "readClipboard"' in bridge, "the bridge must handle the readClipboard request"
handler = bridge[bridge.index('op == "readClipboard"'):bridge.index('op == "emit"')]
assert "SystemClipboard::ReadTextUtf8()" in handler, "readClipboard must resolve with the OS clipboard text"
assert "AddTask" not in handler, "clipboard reads must not be queued onto the game thread"
backend = (ROOT / "src/PrismaUI/UltralightBackend.cpp").read_text(encoding="utf-8")
assert backend.count("platform.set_clipboard(nullptr)") == 2, "both Ultralight teardown paths must detach the Clipboard"
xmake = (ROOT / "xmake.lua").read_text(encoding="utf-8")
assert '"src/PrismaUI/SystemClipboard.cpp"' in xmake, "the FO4VR target links UltralightBackend and must list SystemClipboard.cpp"

subprocess.run(["node", "--test", str(ROOT / "tests" / "prisma_osk_test.js"), str(ROOT / "tests" / "prisma_osk_clipboard_test.js")], cwd=ROOT, check=True)
print("shared on-screen keyboard tests passed")
