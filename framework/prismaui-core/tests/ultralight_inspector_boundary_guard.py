from pathlib import Path


backend = Path("src/PrismaUI/UltralightBackend.cpp").read_text(encoding="utf-8")
bridge = Path("src/PrismaUI/UltralightBackendCallbacks.cpp").read_text(encoding="utf-8")
views = Path("src/PrismaUI/UltralightBackendViews.cpp").read_text(encoding="utf-8")
backend_header = Path("src/PrismaUI/UltralightBackend.h").read_text(encoding="utf-8")
header = Path("src/PrismaUI/UltralightInspector.h").read_text(encoding="utf-8")
source = Path("src/PrismaUI/UltralightInspector.cpp").read_text(encoding="utf-8")
xmake = Path("xmake.lua").read_text(encoding="utf-8")

assert "class IUltralightInspectorHost" in header
assert "class UltralightInspector final" in header
for method in (
    "Add", "RemoveForOwner", "OnInspectorRequestClose", "SetViewport", "SetVisibility",
    "IsVisible", "SetBounds", "BeginMove", "BeginResize", "UpdateGesture", "GestureTargetAt",
    "EndGesture", "MoveBy", "ResizeBy", "GetFrame", "TargetAt", "VisibleView", "InstallHost",
):
    assert f"UltralightInspector::{method}" in source
assert "IUltralightInspectorHost" in backend_header
assert "inspector_(*this)" in backend
assert "UltralightInspector inspector_;" in backend_header
for method in ("SetVisibility", "SetBounds", "BeginMove", "BeginResize", "UpdateGesture", "GetFrame", "TargetAt", "VisibleView"):
    assert f"inspector_.{method}" in views
assert "inspector_.InstallHost(view, inspectorId)" in bridge
assert "struct InspectorState" not in backend
assert "std::map<ViewId, InspectorState>" not in bridge
assert "src/PrismaUI/UltralightInspector.cpp" in xmake
assert len(header.splitlines()) < 500
assert len(source.splitlines()) < 500

print("Ultralight inspector boundary guard passed")
