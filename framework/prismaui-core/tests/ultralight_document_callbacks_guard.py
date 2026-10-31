from pathlib import Path


header = Path("src/PrismaUI/UltralightDocumentCallbacks.h").read_text(encoding="utf-8")
source = Path("src/PrismaUI/UltralightDocumentCallbacks.cpp").read_text(encoding="utf-8")
bridge = Path("src/PrismaUI/UltralightBackendCallbacks.cpp").read_text(encoding="utf-8")
input_source = Path("src/PrismaUI/UltralightBackendInput.cpp").read_text(encoding="utf-8")
xmake = Path("xmake.lua").read_text(encoding="utf-8")

assert "class IUltralightDocumentCallbackHost" in header
assert "class UltralightDocumentCallbacks final" in header
for method in (
    "OnConsole",
    "OnChangeURL",
    "OnBeginLoading",
    "OnFinishLoading",
    "OnFailLoading",
    "OnWindowObjectReady",
    "OnDomReady",
):
    assert f"UltralightDocumentCallbacks::{method}" in source
    assert f"documentCallbacks_.{method}" in bridge

for method in (
    "FindDocumentView",
    "SetDocumentHealth",
    "DocumentFileUrl",
    "ReportDocumentError",
    "DispatchDocument",
    "InstallInspectorHost",
):
    assert f"virtual " in header and method in header
    assert f"{method}" in bridge

for forbidden in ("EvaluateScript", "BlockNavigation", "FlushPendingCalls", "InstallListeners", "ViewHealth::ScriptError"):
    assert forbidden not in bridge

assert "UltralightDocumentCallbacks.cpp" in xmake
assert "documentCallbacks_.InvokeFunction" in input_source
assert len(source.splitlines()) < 500
assert len(header.splitlines()) < 500

print("Ultralight document callback boundary guard passed")
