#!/usr/bin/env python3
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
API = ROOT / "src" / "API"

text = "\n".join((API / name).read_text(encoding="utf-8", errors="replace")
                for name in ("MeshBinding.cpp", "MeshBindingDiag.cpp", "MeshBindingInternal.h"))
COMMENTS_AND_LITERALS = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.S)
code = COMMENTS_AND_LITERALS.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), text)

LOCK = r"std::lock_guard\s+lock\s*\(\s*s_meshBindingsMutex\s*\)"
PUBLISH = r"s_activeBindings\.store\(\s*s_meshBindings\.size\(\)\s*,\s*std::memory_order_release\s*\)"
MUTATION = re.compile(
    r"s_meshBindings\s*(?:\[[^\]]*\]\s*=(?!=)|=(?!=)|\.\s*(?:insert|insert_or_assign|emplace|emplace_hint|"
    r"try_emplace|erase|clear|extract|swap|merge)\s*\([^)]*\))"
)


def fail(message):
    print(f"FAIL: {message}")
    raise SystemExit(1)


def body(name):
    match = re.search(rf"\bvoid\s+{name}\s*\((?:[^()]|\([^()]*\))*\)\s*\{{", code)
    if not match:
        fail(f"src/API/MeshBinding*.cpp no longer defines void {name}()")
    depth = 0
    for index in range(match.end() - 1, len(code)):
        if code[index] == "{":
            depth += 1
        elif code[index] == "}":
            depth -= 1
            if depth == 0:
                return code[match.end() - 1:index + 1]
    fail(f"src/API/MeshBinding*.cpp has unbalanced braces in {name}()")


def require_order(region, name, *patterns):
    positions = []
    for pattern in patterns:
        found = re.search(pattern, region)
        if not found:
            fail(f"{name} in src/API/MeshBinding*.cpp no longer contains {pattern!r}")
        positions.append(found.start())
    if positions != sorted(positions):
        fail(f"{name} in src/API/MeshBinding*.cpp runs its steps out of order")


sites = sorted(" ".join(m.group(0).split()) for m in MUTATION.finditer(code))
if sites != ["s_meshBindings.erase(it)", "s_meshBindings[view] ="]:
    fail(f"s_meshBindings is mutated outside Store() and Unbind(): {sites}")

require_order(body("Store"), "Store()", LOCK, r"s_meshBindings\[view\]\s*=(?!=)", PUBLISH)
require_order(body("Unbind"), "Unbind()", LOCK, r"s_meshBindings\.erase\(\s*it\s*\)", PUBLISH)
require_order(
    body("DiagTick"), "DiagTick()",
    r"\+\+frame\b",
    r"if\s*\(\s*s_activeBindings\.load\(\s*std::memory_order_acquire\s*\)\s*==\s*0\s*\)\s*return\s*;",
    LOCK,
    r"currentSrv\(\s*view\s*\)",
    r"Engine::PokeEngineTextureSRV\(\s*b\.texture\s*,\s*current\.Get\(\)\s*\)",
    r"b\.srvRef\s*=\s*current\s*;",
    r"if\s*\(\s*!probe\s*\)\s*return\s*;",
)

print("mesh_binding_fast_path_guard ok (active-binding count is published under the mutex; "
      "DiagTick returns before the lock when no binding exists)")
