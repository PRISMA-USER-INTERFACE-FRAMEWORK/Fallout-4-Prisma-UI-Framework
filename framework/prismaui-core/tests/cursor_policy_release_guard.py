from pathlib import Path

root = Path(__file__).resolve().parents[1]
vr_header = (root / "src" / "PrismaUI_F4VR_API.h").read_text(encoding="utf-8")
policy_h = (root / "src" / "PrismaUI" / "ViewCursorPolicy.h").read_text(encoding="utf-8")
vr_stubs = (root / "src" / "VR" / "VRUnportedStubs.cpp").read_text(encoding="utf-8")
release_script = (root / "scripts" / "ci-package-release.py").read_text(encoding="utf-8")

assert '#include "PrismaUI_F4_API.h"' in vr_header
assert "inline void SetPolicy" in policy_h
assert "inline bool ShouldDrawCompositorCursor" in policy_h
assert "namespace PrismaUI::MenuCursorDiagnostics" in vr_stubs
assert "void OnFocusedCaptureArmed(" in vr_stubs
assert "void OnPresentCursorSample(" in vr_stubs
assert '"PrismaUI_F4_GetCapabilities"' in release_script
assert '"PrismaUI_F4_SetViewCursorPolicy"' in release_script
assert '"PrismaUI_F4_GetViewCursorPolicy"' in release_script

print("cursor policy VR/release guard passed")
