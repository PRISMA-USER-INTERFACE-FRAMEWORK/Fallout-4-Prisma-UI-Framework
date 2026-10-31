#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path

KINDS = ("img", "mask", "webkit-mask", "inline")
SIZES = (13, 16, 20, 24)
EXPECTED_KEYS = {(kind, size) for kind in KINDS for size in SIZES}
CASE = re.compile(
    r"^raster_case kind=(?P<kind>\S+) size=(?P<size>\d+) supported=(?P<supported>[01]) "
    r"lit_pixels=(?P<lit>\d+) energy=(?P<energy>[0-9]+(?:\.[0-9]+)?)$"
)


@dataclass(frozen=True)
class Result:
    supported: bool
    lit_pixels: int
    energy: float


def format_keys(keys: set[tuple[str, int]]) -> str:
    return ", ".join(f"{kind}@{size}" for kind, size in sorted(keys)) or "none"


def parse(path: Path) -> dict[tuple[str, int], Result]:
    results: dict[tuple[str, int], Result] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        match = CASE.match(line.strip())
        if not match:
            continue
        key = (match.group("kind"), int(match.group("size")))
        if key in results:
            raise RuntimeError(f"duplicate raster case in {path}: {key}")
        results[key] = Result(
            supported=match.group("supported") == "1",
            lit_pixels=int(match.group("lit")),
            energy=float(match.group("energy")),
        )
    actual_keys = set(results)
    if actual_keys != EXPECTED_KEYS:
        missing = EXPECTED_KEYS - actual_keys
        extra = actual_keys - EXPECTED_KEYS
        raise RuntimeError(
            f"raster case set mismatch in {path}: missing={format_keys(missing)} extra={format_keys(extra)}"
        )
    return results


def reference_ok(result: Result) -> bool:
    return result.supported and result.lit_pixels > 0 and result.energy > 1.0


def baseline_losses(results: dict[tuple[str, int], Result]) -> list[tuple[str, int]]:
    for kind in ("img", "inline"):
        if not reference_ok(results[(kind, 24)]):
            raise RuntimeError(f"24px reference failed for {kind}")
    return [key for key, value in results.items() if key[1] == 13 and value.supported and value.lit_pixels == 0]


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate or compare Ultralight SVG raster measurements from the exact same fixture")
    parser.add_argument("--expect-no-repro", action="store_true")
    parser.add_argument("baseline", type=Path)
    parser.add_argument("candidate", type=Path, nargs="?")
    args = parser.parse_args()

    if args.expect_no_repro and args.candidate is not None:
        parser.error("--expect-no-repro accepts a baseline log only")

    baseline = parse(args.baseline)
    lost_13px = baseline_losses(baseline)

    if args.expect_no_repro:
        unsupported = {key for key, value in baseline.items() if not value.supported}
        if unsupported:
            raise RuntimeError("baseline support matrix changed: unsupported=" + format_keys(unsupported))
        if lost_13px:
            raise RuntimeError(
                "baseline unexpectedly reproduces 13px loss in: " + ", ".join(kind for kind, _ in lost_13px)
            )
        print("baseline no-reproduction characterization confirmed")
        return 0

    if not lost_13px:
        raise RuntimeError("baseline does not reproduce a supported 13px disappearance")
    print("baseline reproduced 13px loss in: " + ", ".join(kind for kind, _ in lost_13px))

    if args.candidate is None:
        return 0

    candidate = parse(args.candidate)
    for key in sorted(EXPECTED_KEYS):
        if baseline[key].supported != candidate[key].supported:
            raise RuntimeError(f"CSS support matrix changed for {key}: baseline={baseline[key].supported} candidate={candidate[key].supported}")

    for kind in ("img", "inline"):
        if not reference_ok(candidate[(kind, 24)]):
            raise RuntimeError(f"candidate 24px reference failed for {kind}")

    for key in lost_13px:
        value = candidate[key]
        if value.lit_pixels == 0 or value.energy <= 0.0:
            raise RuntimeError(f"candidate still loses the baseline 13px case: {key}")

    print("candidate restored every reproduced 13px case")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
