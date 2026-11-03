from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "src/PrismaUI"
BACKEND = (SOURCE / "UltralightBackend.cpp").read_text(encoding="utf-8")
HEADER = (SOURCE / "UltralightBackend.h").read_text(encoding="utf-8")
XMAKE = (ROOT / "xmake.lua").read_text(encoding="utf-8")

assert not list(SOURCE.glob("UltralightBackend*.inl")), "backend implementation fragments must not return"
assert not (ROOT / "tests/backend_source.py").exists(), "monolithic backend reconstruction must stay retired"
assert ".inl" not in BACKEND, "the backend root must not include textual implementation fragments"
assert "class UltralightBackend final" in HEADER
assert "class UltralightBackend final" not in BACKEND
assert len(BACKEND.splitlines()) < 500
assert len(HEADER.splitlines()) < 500

MODULES = (
    "UltralightBackend.cpp",
    "UltralightBackendCallbacks.cpp",
    "UltralightBackendDispatch.cpp",
    "UltralightBackendFrames.cpp",
    "UltralightBackendInput.cpp",
    "UltralightBackendPresentation.cpp",
    "UltralightBackendViewState.cpp",
    "UltralightBackendViews.cpp",
)
for module in MODULES:
    source = SOURCE / module
    assert source.is_file(), f"missing backend responsibility module: {module}"
    assert len(source.read_text(encoding="utf-8").splitlines()) < 500, f"{module} exceeds the source-size policy"
    if module != "UltralightBackend.cpp":
        assert '#include "UltralightBackend.h"' in source.read_text(encoding="utf-8")
    assert f'"src/PrismaUI/{module}"' in XMAKE or module == "UltralightBackend.cpp"

for module in MODULES[1:]:
    source = (SOURCE / module).read_text(encoding="utf-8")
    assert "#include \"UltralightBackend" not in source.replace('#include "UltralightBackend.h"', "")

print("Ultralight backend architecture guard passed")
