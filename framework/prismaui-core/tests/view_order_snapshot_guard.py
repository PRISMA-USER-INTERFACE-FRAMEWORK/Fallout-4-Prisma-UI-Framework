#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
PRESENTATION = ROOT / "src" / "PrismaUI" / "WebRuntimePresentation.cpp"
ORDERED_SOURCES = (
    PRESENTATION,
    ROOT / "src" / "PrismaUI" / "WebRuntime.cpp",
    ROOT / "src" / "VR" / "VRCompositor.cpp",
)
SNAPSHOT = "orderedViews.emplace_back(RuntimeViewOrder(view), view);"
FIRST_ONLY = re.compile(
    r"\[\]\s*\(const auto& (\w+), const auto& (\w+)\)\s*\{\s*return \1\.first < \2\.first;\s*\}"
)
LIVE_LOOKUP = re.compile(r"\b(GetOrder|RuntimeViewOrder)\(")


def sort_calls(text):
    calls = []
    for match in re.finditer(r"std::sort\(", text):
        depth = 0
        for at in range(match.end() - 1, len(text)):
            if text[at] == "(":
                depth += 1
            elif text[at] == ")":
                depth -= 1
                if depth == 0:
                    calls.append(text[match.start():at + 1])
                    break
    return calls


failures = []
for path in ORDERED_SOURCES:
    for call in sort_calls(path.read_text(encoding="utf-8")):
        for lookup in LIVE_LOOKUP.finditer(call):
            failures.append(f"{path.relative_to(ROOT).as_posix()}: {lookup.group(1)}( inside a std::sort comparator")

presentation = PRESENTATION.read_text(encoding="utf-8")
comparators = sort_calls(presentation)
if len(comparators) != 2:
    failures.append(f"expected 2 std::sort calls in WebRuntimePresentation.cpp, found {len(comparators)}")
for call in comparators:
    if not FIRST_ONLY.search(call):
        failures.append("a WebRuntimePresentation.cpp std::sort comparator is not a '.first < .first' compare")
if presentation.count("RuntimeViewOrder(") != 2 or presentation.count(SNAPSHOT) != 2:
    failures.append("RuntimeViewOrder( must appear only in the two snapshot loops of WebRuntimePresentation.cpp")

if failures:
    for failure in failures:
        print(f"FAIL: {failure}")
    raise SystemExit(1)
print("view_order_snapshot_guard ok (sort comparators read the captured .first snapshot)")
