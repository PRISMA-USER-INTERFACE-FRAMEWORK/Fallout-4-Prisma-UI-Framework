#!/usr/bin/env python3
"""Check a plugin's copy of PrismaUI_F4_API.h against this one.

The API is a set of vtable interfaces, so a plugin calls each method by slot number. Insert a
method anywhere but the end of the newest interface and every later slot shifts: a plugin built
against the old header then calls the wrong function, silently. This checks a header copy is safe.

    python3 scripts/check_api_prefix.py path/to/your/PrismaUI_F4_API.h

The rule is not a simple per-interface prefix. Interfaces share one flat vtable, so a header that
is only missing an interface's trailing methods is safe just for the newest interface it declares;
missing them from an inherited interface shifts every derived slot. Exit 0 safe, 1 slot shift, 2
could not run.
"""

import argparse
import pathlib
import re
import sys

EXIT_OK, EXIT_MISMATCH, EXIT_USAGE = 0, 1, 2


def die(message):
    """Exit 2 for a usage/parse failure, distinct from exit 1 for a real mismatch."""
    print(f"error: {message}", file=sys.stderr)
    sys.exit(EXIT_USAGE)

INTERFACE_RE = re.compile(
    r"\bclass\s+(IVPrismaUI\d+)\s*(?::\s*public\s+(IVPrismaUI\d+)\s*)?\{"
)
# A declaration runs from `virtual` to the first `;` -- several span multiple lines.
VIRTUAL_RE = re.compile(r"\bvirtual\b[^;]*;", re.DOTALL)


def strip_comments(text):
    """Remove // and /* */ comments so commentary never counts as a declaration.

    String and char literals are preserved: a // inside one would otherwise eat the rest of the
    line. The header has none inside declarations today, but a future default argument could.
    """
    out = []
    i, n = 0, len(text)
    while i < n:
        two = text[i : i + 2]
        if two == "//":
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif two == "/*":
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
        elif text[i] in "\"'":
            quote = text[i]
            out.append(text[i])
            i += 1
            while i < n:
                if text[i] == "\\":
                    out.append(text[i : i + 2])
                    i += 2
                    continue
                out.append(text[i])
                if text[i] == quote:
                    i += 1
                    break
                i += 1
        else:
            out.append(text[i])
            i += 1
    return "".join(out)


def class_body(text, open_brace_index):
    """Return the source between the class's braces, honouring nesting."""
    depth = 0
    for i in range(open_brace_index, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[open_brace_index + 1 : i]
    die("unterminated class body -- is the header truncated?")


def normalize(decl):
    """Collapse a declaration to a comparable form.

    Whitespace and `virtual`/`noexcept`/`= 0` decoration are normalised away, but PARAMETER TYPES
    are kept: a slot whose signature changed is just as wrong to call as a slot that moved, and it
    is the harder one to notice by eye.
    """
    decl = re.sub(r"\s+", " ", decl).strip()
    decl = re.sub(r"^virtual\s+", "", decl)
    decl = re.sub(r"\s*=\s*0\s*;$", "", decl)
    decl = re.sub(r";$", "", decl)
    decl = re.sub(r"\bnoexcept\b", "", decl)
    decl = re.sub(r"\s*([(),*&])\s*", r"\1", decl)
    return re.sub(r"\s+", " ", decl).strip()


def parse_interfaces(path):
    """Return {interface: {"base": str|None, "methods": [normalized, ...]}} in declaration order."""
    try:
        text = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
    except OSError as exc:
        die(f"cannot read {path}: {exc}")

    interfaces = {}
    for match in INTERFACE_RE.finditer(text):
        name, base = match.group(1), match.group(2)
        body = class_body(text, match.end() - 1)
        methods = [normalize(d) for d in VIRTUAL_RE.findall(body)]
        interfaces[name] = {"base": base, "methods": methods}
    return interfaces


def version_of(name):
    return int(name[len("IVPrismaUI") :])


def has_declared_descendant(name, consumer):
    """True if any interface the CONSUMER declares derives, transitively, from `name`.

    This is the whole subtlety of the check. Each interface's methods sit after its base's in one
    flat vtable, so a consumer that is merely MISSING TRAILING METHODS of an inherited interface is
    NOT safe: every slot of every derived interface shifts up by the shortfall. e.g. a header whose
    IVPrismaUI9 is missing its two newest methods while IVPrismaUI10 is current has every V10 call
    land two slots early, inside V9.

    A proper prefix is tolerable only for an interface nothing else derives from (the newest one the
    header declares, whose trailing methods it never calls). Everything below it must match exactly.
    """
    for other, info in consumer.items():
        base = info["base"]
        while base is not None:
            if base == name:
                return True
            base = consumer.get(base, {}).get("base") if base in consumer else None
    return False


def compare(consumer, framework):
    """Return a list of human-readable problems. Empty means the consumer is safe."""
    problems = []

    for name in sorted(consumer, key=version_of):
        if name not in framework:
            problems.append(
                f"{name}: declared by the consumer but NOT by the framework. The consumer is "
                f"ahead of the framework, or this interface was renamed/removed. Either way the "
                f"vtable it expects does not exist."
            )
            continue

        mine, theirs = consumer[name], framework[name]

        if mine["base"] != theirs["base"]:
            problems.append(
                f"{name}: inherits from {mine['base'] or '(nothing)'} in the consumer but "
                f"{theirs['base'] or '(nothing)'} in the framework. Every slot in this interface "
                f"is offset by the difference in the base's method count."
            )

        for i, decl in enumerate(mine["methods"]):
            if i >= len(theirs["methods"]):
                problems.append(
                    f"{name}: slot {i} is '{decl}' in the consumer, but the framework's "
                    f"{name} only has {len(theirs['methods'])} method(s). Calling it would run "
                    f"off the end of the vtable."
                )
                break
            if decl != theirs["methods"][i]:
                problems.append(
                    f"{name}: SLOT {i} DISAGREES -- a call here lands on the wrong function.\n"
                    f"      consumer:  {decl}\n"
                    f"      framework: {theirs['methods'][i]}\n"
                    f"      Either a method was inserted into {name} rather than appended to the "
                    f"newest interface, or this slot's signature changed. Both are breaking."
                )
                break
        else:
            # Same methods as far as the consumer goes. That is only SAFE if nothing the consumer
            # declares derives from this interface -- otherwise the missing tail shifts every
            # derived slot. See has_declared_descendant.
            missing = len(theirs["methods"]) - len(mine["methods"])
            if missing > 0 and has_declared_descendant(name, consumer):
                derived = sorted(
                    (o for o in consumer if has_ancestor(o, name, consumer)), key=version_of
                )
                problems.append(
                    f"{name}: STALE, and it has a derived interface -- every slot in "
                    f"{', '.join(derived)} shifts by {missing}.\n"
                    f"      The consumer's {name} has {len(mine['methods'])} methods; the "
                    f"framework's has {len(theirs['methods'])}.\n"
                    f"      Missing: {', '.join(theirs['methods'][len(mine['methods']):])}\n"
                    f"      A missing TRAILING method is harmless only in the newest interface a "
                    f"consumer declares. Here it is inherited, so re-copy the header."
                )

    return problems


def has_ancestor(name, ancestor, interfaces):
    base = interfaces.get(name, {}).get("base")
    while base is not None:
        if base == ancestor:
            return True
        base = interfaces.get(base, {}).get("base") if base in interfaces else None
    return False


def main():
    parser = argparse.ArgumentParser(
        description="Verify a consumer's PrismaUI_F4_API.h is vtable-compatible with the framework's.",
    )
    parser.add_argument("consumer", type=pathlib.Path, help="the plugin's copy of PrismaUI_F4_API.h")
    parser.add_argument(
        "--framework",
        type=pathlib.Path,
        default=pathlib.Path(__file__).resolve().parent.parent / "src" / "PrismaUI_F4_API.h",
        help="the framework's header (defaults to this repo's src/PrismaUI_F4_API.h)",
    )
    args = parser.parse_args()

    for path, label in ((args.consumer, "consumer"), (args.framework, "framework")):
        if not path.is_file():
            die(f"{label} header not found: {path}")

    consumer = parse_interfaces(args.consumer)
    framework = parse_interfaces(args.framework)

    # A header that parses to nothing is a silent pass otherwise -- the most dangerous outcome here,
    # since "no problems found" is exactly what a caller is hoping to see.
    if not framework:
        die(f"no IVPrismaUI* interfaces found in {args.framework} -- wrong file?")
    if not consumer:
        die(f"no IVPrismaUI* interfaces found in {args.consumer} -- wrong file?")

    problems = compare(consumer, framework)

    print(f"consumer:  {args.consumer}")
    print(f"framework: {args.framework}")
    print()
    for name in sorted(framework, key=version_of):
        theirs = len(framework[name]["methods"])
        if name not in consumer:
            print(f"  {name:<14} -- not in consumer ({theirs} methods) -- fine, consumer is older")
            continue
        mine = len(consumer[name]["methods"])
        if any(p.startswith(name + ":") for p in problems):
            state = "MISMATCH"
        elif mine == theirs:
            state = "exact"
        else:
            state = f"prefix, safe ({theirs - mine} newer method(s) the consumer never calls)"
        print(f"  {name:<14} {mine}/{theirs} methods -- {state}")
    print()

    if problems:
        print(f"FAIL: {len(problems)} problem(s) -- this consumer header would call wrong slots.\n")
        for p in problems:
            print(f"  * {p}")
        print(
            "\nFix by re-copying the framework's header. If the framework is the side that moved a"
            "\nmethod, that is the bug, not the consumer's: append to the NEWEST interface or add a"
            "\nnew one. An interface that has a subclass is closed."
        )
        return EXIT_MISMATCH

    print("OK: every shared interface is a prefix match. No slot can shift.")
    return EXIT_OK


if __name__ == "__main__":
    sys.exit(main())
