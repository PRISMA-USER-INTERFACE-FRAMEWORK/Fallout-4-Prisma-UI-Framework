from pathlib import Path


header = Path("src/PrismaUI/UltralightViewCallbacks.h").read_text(encoding="utf-8")
source = Path("src/PrismaUI/UltralightViewCallbacks.cpp").read_text(encoding="utf-8")
backend = Path("src/PrismaUI/UltralightBackend.cpp").read_text(encoding="utf-8")
backend_header = Path("src/PrismaUI/UltralightBackend.h").read_text(encoding="utf-8")

for name in (
    "OnConsole",
    "OnChangeURL",
    "OnBeginLoading",
    "OnFinishLoading",
    "OnFailLoading",
    "OnWindowObjectReady",
    "OnDomReady",
    "OnCreateInspectorView",
    "OnInspectorRequestClose",
):
    assert f"virtual " in header and name in header
    assert f"sink_.{name}" in source

assert "class UltralightViewCallbacks final" in header
assert "UltralightViewCallbacks" in backend_header
assert "class ViewCallbacks" not in backend
assert "backend_." not in source

print("Ultralight view callback boundary guard passed")
