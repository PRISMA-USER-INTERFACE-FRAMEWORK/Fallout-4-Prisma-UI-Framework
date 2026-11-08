#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parent.parent
callbacks = (root / "src/PrismaUI/WebRuntimeCallbacks.cpp").read_text(encoding="utf-8")


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError(f"unclosed function: {signature}")


interop = function_body(callbacks, "void InteropCall(ViewId view")
validity = interop.find("!IsValid(view)")
dispatch = interop.find("st.backend->CallFunction(view")
assert validity != -1, "InteropCall must reject a dead view before backend dispatch"
assert dispatch != -1 and validity < dispatch, "InteropCall validity check must precede backend dispatch"

print("runtime dead-handle guard passed")
