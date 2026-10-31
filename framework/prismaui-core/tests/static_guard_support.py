from pathlib import Path

failures: list[str] = []


def check(name: str):
    def wrap(fn):
        try:
            fn()
            print(f"  ok    {name}")
        except AssertionError as exc:
            print(f"  FAIL  {name}: {exc}")
            failures.append(name)
        except Exception as exc:
            print(f"  ERROR {name}: {type(exc).__name__}: {exc}")
            failures.append(name)
        return fn
    return wrap


def read(path: str) -> str:
    return Path(path).read_text(encoding="utf-8")


def read_web_runtime() -> str:
    parts = (
        "WebRuntime.cpp",
        "WebRuntimeInternal.h",
        "WebRuntimeState.cpp",
        "WebRuntimeLifecycle.cpp",
        "WebRuntimeFrame.cpp",
        "WebRuntimePresent.cpp",
        "WebRuntimePresentation.cpp",
        "WebRuntimeFocus.cpp",
        "WebRuntimeInput.cpp",
        "WebRuntimeViews.cpp",
        "WebRuntimeInspector.cpp",
        "WebRuntimeCallbacks.cpp",
    )
    return "\n".join(read(f"src/PrismaUI/{part}") for part in parts)


def function_body(source: str, signature: str) -> str:
    start = -1
    cursor = 0
    while True:
        found = source.find(signature, cursor)
        if found == -1:
            break
        brace = source.find("{", found)
        semicolon = source.find(";", found)
        if brace != -1 and (semicolon == -1 or brace < semicolon):
            start = found
            break
        cursor = found + len(signature)
    if start == -1:
        raise AssertionError(f"no definition found for: {signature}")
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
