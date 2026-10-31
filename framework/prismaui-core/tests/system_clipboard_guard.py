from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/PrismaUI/SystemClipboard.cpp").read_text(encoding="utf-8")


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


write = function_body(SOURCE, "void WritePlainText(")
for needle in ("EmptyClipboard", "SetClipboardData(CF_UNICODETEXT", "GlobalFree", "CloseClipboard"):
    assert needle in write, f"WritePlainText must contain {needle}"
assert "wcslen" not in write, "WritePlainText must use the explicit length, not wcslen"
assert write.index("GlobalUnlock") < write.index("EmptyClipboard"), "WritePlainText must fill the block before emptying the clipboard"

clear = function_body(SOURCE, "void Clear()")
assert "EmptyClipboard" in clear and "CloseClipboard" in clear, "Clear must empty and close the clipboard"

print("system clipboard write guard ok")
