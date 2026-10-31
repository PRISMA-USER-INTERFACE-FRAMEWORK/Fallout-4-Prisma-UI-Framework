#!/usr/bin/env python3
"""Keep the PrismaUI Papyrus native registration, source, and shipped PEX in sync."""
from pathlib import Path
import struct
import sys

EXPECTED = {
    "Focus": ("Bool", (("asName", "String"), ("abPauseGame", "Bool"), ("abDisableFocusMenu", "Bool"))),
    "Unfocus": ("None", (("asName", "String"),)),
    "HasFocus": ("Bool", (("asName", "String"),)),
}


def u16(data: bytes, off: int) -> int:
    return struct.unpack_from("<H", data, off)[0]


def u32(data: bytes, off: int) -> int:
    return struct.unpack_from("<I", data, off)[0]


def read_wstr(data: bytes, off: int) -> tuple[str, int]:
    size = u16(data, off)
    off += 2
    return data[off:off + size].decode("utf-8"), off + size


def skip_function(data: bytes, off: int) -> tuple[dict, int]:
    name = u16(data, off)
    ret = u16(data, off + 2)
    doc = u16(data, off + 4)
    flags = data[off + 10]
    off += 11

    param_count = u16(data, off)
    off += 2
    params = []
    for _ in range(param_count):
        params.append((u16(data, off), u16(data, off + 2)))
        off += 4

    local_count = u16(data, off)
    off += 2
    off += local_count * 4

    instruction_count = u16(data, off)
    off += 2
    if instruction_count != 0:
        raise AssertionError("PrismaUI.pex gained Papyrus bytecode; update this guard before parsing instructions")

    return {"name": name, "ret": ret, "doc": doc, "flags": flags, "params": params}, off


def parse_native_functions(data: bytes) -> tuple[list[str], dict[str, tuple[str, tuple[tuple[str, str], ...]]]]:
    if len(data) < 16 or u32(data, 0) != 0xFA57C0DE:
        raise AssertionError("scripts/PrismaUI.pex is not a Fallout PEX file")
    if data[4] != 3 or data[5] != 9 or u16(data, 6) != 2:
        raise AssertionError("unexpected PrismaUI.pex version/game id")

    off = 16
    _, off = read_wstr(data, off)  # source path
    _, off = read_wstr(data, off)  # user
    _, off = read_wstr(data, off)  # machine

    string_count = u16(data, off)
    off += 2
    strings = []
    for _ in range(string_count):
        value, off = read_wstr(data, off)
        strings.append(value)

    has_debug = data[off]
    off += 1
    if has_debug:
        off += 8  # modification time
        debug_count = u16(data, off)
        off += 2
        for _ in range(debug_count):
            off += 7
            line_count = u16(data, off)
            off += 2 + line_count * 2

        property_group_count = u16(data, off)
        off += 2
        if property_group_count:
            raise AssertionError("PrismaUI.pex gained debug property groups; update this guard")
        struct_order_count = u16(data, off)
        off += 2
        if struct_order_count:
            raise AssertionError("PrismaUI.pex gained debug struct orders; update this guard")

    user_flag_count = u16(data, off)
    off += 2 + user_flag_count * 3

    object_count = u16(data, off)
    off += 2
    if object_count != 1:
        raise AssertionError(f"expected one PrismaUI script object, found {object_count}")

    _object_name = u16(data, off)
    object_size = u32(data, off + 2)
    off += 6
    object_start = off

    off += 11  # parent, doc, user flags, object flags, auto state
    struct_count = u16(data, off)
    off += 2
    if struct_count:
        raise AssertionError("PrismaUI.pex gained structs; update this guard")
    variable_count = u16(data, off)
    off += 2
    if variable_count:
        raise AssertionError("PrismaUI.pex gained variables; update this guard")
    property_count = u16(data, off)
    off += 2
    if property_count:
        raise AssertionError("PrismaUI.pex gained properties; update this guard")
    state_count = u16(data, off)
    off += 2
    if state_count != 1:
        raise AssertionError(f"expected one native state, found {state_count}")

    off += 2  # state name
    function_count = u16(data, off)
    off += 2
    functions = {}
    for _ in range(function_count):
        fn, off = skip_function(data, off)
        name = strings[fn["name"]]
        ret = strings[fn["ret"]]
        params = tuple((strings[n], strings[t]) for n, t in fn["params"])
        if fn["flags"] != 3:
            raise AssertionError(f"{name} is no longer a global native function (flags={fn['flags']})")
        functions[name] = (ret, params)

    if off - object_start != object_size:
        raise AssertionError("PrismaUI.pex object size field does not match parsed native table")
    if off != len(data):
        raise AssertionError("PrismaUI.pex has unparsed trailing data")
    return strings, functions


def main() -> int:
    cpp = Path("src/PrismaUI/PapyrusVM.cpp").read_text(encoding="utf-8")
    psc = Path("scripts/Source/PrismaUI.psc").read_text(encoding="utf-8")
    pex = Path("scripts/PrismaUI.pex").read_bytes()
    _, functions = parse_native_functions(pex)

    for name, signature in EXPECTED.items():
        assert f'BindNativeMethod(kScript, "{name}"' in cpp, f"C++ registration missing {name}"
        assert f"Function {name}(" in psc, f"Papyrus source declaration missing {name}"
        assert functions.get(name) == signature, (
            f"shipped PEX signature for {name} is {functions.get(name)!r}, expected {signature!r}"
        )

    print("papyrus API guard passed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, IndexError, UnicodeDecodeError, struct.error) as exc:
        print(f"papyrus API guard failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
