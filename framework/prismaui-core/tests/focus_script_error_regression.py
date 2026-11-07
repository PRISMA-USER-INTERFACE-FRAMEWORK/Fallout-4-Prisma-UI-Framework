from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[3]
SCRIPT_ERROR_HEALTH = (
    re.compile(r"(SetHealth|SetDocumentHealth)\([^;]*ScriptError"),
    re.compile(r"health\s*=(?!=)[^;]*ScriptError"),
    re.compile(r"health_\[[^\]]*\]\s*=\s*[^;]*ScriptError"),
)


def function_body(source: str, marker: str) -> str:
    start = source.index(marker)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise AssertionError(f"unterminated function: {marker}")


def assert_no_script_error_health() -> None:
    for path in sorted((ROOT / "framework" / "prismaui-core" / "src").rglob("*")):
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for pattern in SCRIPT_ERROR_HEALTH:
            match = pattern.search(text)
            assert match is None, f"{path.name} sets lifecycle health to ScriptError: {match and match.group(0)}"


def run(source_path: Path | None = None) -> None:
    assert_no_script_error_health()
    if source_path is None:
        sources = {
            "EvaluateScript": Path("src/PrismaUI/UltralightBackendInput.cpp").read_text(encoding="utf-8"),
            "RegisterLocalizationScript": Path("src/PrismaUI/UltralightBackendInput.cpp").read_text(encoding="utf-8"),
            "OnWindowObjectReady": Path("src/PrismaUI/UltralightDocumentCallbacks.cpp").read_text(encoding="utf-8"),
        }
    else:
        source = source_path.read_text(encoding="utf-8")
        sources = {name: source for name in (
            "EvaluateScript", "RegisterLocalizationScript", "OnWindowObjectReady")}
    script_paths = {
        "EvaluateScript": "void UltralightBackend::EvaluateScript(",
        "RegisterLocalizationScript": "void UltralightBackend::RegisterLocalizationScript(",
        "OnWindowObjectReady": "void UltralightDocumentCallbacks::OnWindowObjectReady(",
    }
    focus = (ROOT / "framework" / "prismaui-core" / "src" / "PrismaUI" / "WebRuntimeFocus.cpp").read_text(
        encoding="utf-8"
    )
    lifecycle = (ROOT / "framework" / "prismaui-core" / "src" / "PrismaUI" / "WebRuntimeLifecycle.cpp").read_text(
        encoding="utf-8"
    )

    for name, marker in script_paths.items():
        body = function_body(sources[name], marker)
        if name == "OnWindowObjectReady":
            assert "ReportError(id, PrismaUI::Web::ViewHealth::ScriptError" in body, f"{name} lost script diagnostics"
            assert "SetDocumentHealth(*record, PrismaUI::Web::ViewHealth::ScriptError" not in body, f"{name} poisons lifecycle health"
        else:
            assert "ReportError(id, PrismaUI::Web::ViewHealth::ScriptError" in body, f"{name} lost script diagnostics"
            assert "SetHealth(*it->second, ViewHealth::ScriptError)" not in body, f"{name} poisons lifecycle health"
    assert "health != PrismaUI::Web::ViewHealth::DomReady" in focus
    assert "health != PrismaUI::Web::ViewHealth::Live" in focus
    assert "const bool isLive = health == static_cast<int>(PrismaUI::Web::ViewHealth::Live);" in lifecycle
    assert "ShouldEmergencyReleaseFocus" in lifecycle


if __name__ == "__main__":
    run(Path(sys.argv[1]) if len(sys.argv) > 1 else None)
    print("script-error focus source contract passed")
