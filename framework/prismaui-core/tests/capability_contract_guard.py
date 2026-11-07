from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src" / "PrismaUI_F4_API.h").read_text(encoding="utf-8")
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
PACKAGE = (ROOT / "scripts" / "ci-package-release.py").read_text(encoding="utf-8")


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        character = source[index]
        if character == "{":
            depth += 1
        elif character == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError(f"unclosed function: {signature}")


capability_enum = re.search(
    r"enum class PrismaCapability\s*:\s*uint64_t\s*\{(.*?)\};",
    HEADER,
    flags=re.DOTALL,
)
assert capability_enum, "missing PrismaCapability declaration"
normalized_enum = re.sub(r"\s+", "", capability_enum.group(1))
assert normalized_enum == "InputRegions=1ull<<0,", "public capability bit assignment changed"

get_capabilities = function_body(HEADER, "[[nodiscard]] inline uint64_t GetPrismaCapabilities() noexcept")
assert 'GetProcAddress(pluginHandle, "PrismaUI_F4_GetCapabilities")' in get_capabilities
assert "if (!pluginHandle)" in get_capabilities and "return 0;" in get_capabilities
assert "return getCapabilities ? getCapabilities() : 0;" in get_capabilities

has_capability = function_body(
    HEADER,
    "[[nodiscard]] inline bool HasPrismaCapability(PrismaCapability capability) noexcept",
)
assert "(GetPrismaCapabilities() & static_cast<uint64_t>(capability)) != 0" in has_capability

provider = function_body(MAIN, 'extern "C" __declspec(dllexport) uint64_t PrismaUI_F4_GetCapabilities()')
assert provider.strip() == "return static_cast<uint64_t>(PRISMA_UI_API::PrismaCapability::InputRegions);"

assert '"PrismaUI_F4_GetCapabilities"' in PACKAGE

print("capability contract guard passed")
