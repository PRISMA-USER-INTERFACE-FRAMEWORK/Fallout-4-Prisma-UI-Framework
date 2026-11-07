from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCES = {
    "frames": (ROOT / "src/PrismaUI/UltralightBackendFrames.cpp").read_text(encoding="utf-8"),
    "input": (ROOT / "src/PrismaUI/UltralightBackendInput.cpp").read_text(encoding="utf-8"),
    "view_state": (ROOT / "src/PrismaUI/UltralightBackendViewState.cpp").read_text(encoding="utf-8"),
}
OWNER_ONLY = (
    ("frames", "void UltralightBackend::PublishFrames()"),
    ("input", "void UltralightBackend::ReleaseControllerKeys("),
    ("input", "bool UltralightBackend::PrepareGamepad("),
    ("input", "void UltralightBackend::PublishGamepad()"),
    ("view_state", "void UltralightBackend::RemoveOwnerView("),
)


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError(f"unclosed function: {signature}")


for source_name, signature in OWNER_ONLY:
    body = function_body(SOURCES[source_name], signature).lstrip()
    assert body.startswith("assert(dispatchQueue_.IsOwnerThread());"), f"{signature} must assert owner-thread first"

print("ultralight owner-thread assert guard ok")
