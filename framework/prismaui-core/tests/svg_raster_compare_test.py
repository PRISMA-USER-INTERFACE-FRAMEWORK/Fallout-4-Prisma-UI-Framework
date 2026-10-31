from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tests/svg_hairline/tools/compare-svg-raster-results.py"
KINDS = ("img", "mask", "webkit-mask", "inline")
SIZES = (13, 16, 20, 24)


def write_result(path: Path, lost_13: set[str], unsupported: set[str] | None = None) -> None:
    unsupported = unsupported or set()
    lines = ["raster_runtime label=test device_scale=1.0"]
    for kind in KINDS:
        for size in SIZES:
            supported = 0 if kind in unsupported else 1
            lit = 0 if not supported or size == 13 and kind in lost_13 else max(1, size * 2)
            energy = 0.0 if lit == 0 else float(lit) / 4.0
            lines.append(
                f"raster_case kind={kind} size={size} supported={supported} "
                f"lit_pixels={lit} energy={energy:.3f}"
            )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


with tempfile.TemporaryDirectory(prefix="prisma-svg-raster-compare-") as temporary:
    root = Path(temporary)
    baseline = root / "baseline.txt"
    fixed = root / "fixed.txt"
    still_broken = root / "still-broken.txt"
    support_regressed = root / "support-regressed.txt"
    no_repro = root / "no-repro.txt"
    no_repro_support_drift = root / "no-repro-support-drift.txt"
    wrong_cases = root / "wrong-cases.txt"
    faint_reference = root / "faint-reference.txt"

    write_result(baseline, {"img", "mask"})
    write_result(fixed, set())
    write_result(still_broken, {"mask"})
    write_result(support_regressed, set(), {"mask"})
    write_result(no_repro, set())
    write_result(no_repro_support_drift, set(), {"mask"})
    write_result(wrong_cases, set())
    write_result(faint_reference, set())
    wrong_cases.write_text(
        wrong_cases.read_text(encoding="utf-8").replace("kind=inline size=24", "kind=bogus size=24"),
        encoding="utf-8",
    )
    faint_reference.write_text(
        faint_reference.read_text(encoding="utf-8").replace(
            "kind=inline size=24 supported=1 lit_pixels=48 energy=12.000",
            "kind=inline size=24 supported=1 lit_pixels=48 energy=0.500",
        ),
        encoding="utf-8",
    )

    baseline_only = subprocess.run([sys.executable, str(SCRIPT), str(baseline)], capture_output=True, text=True)
    assert baseline_only.returncode == 0, baseline_only.stderr
    assert "baseline reproduced 13px loss" in baseline_only.stdout

    success = subprocess.run([sys.executable, str(SCRIPT), str(baseline), str(fixed)], capture_output=True, text=True)
    assert success.returncode == 0, success.stderr
    assert "candidate restored every reproduced 13px case" in success.stdout

    failure = subprocess.run(
        [sys.executable, str(SCRIPT), str(baseline), str(still_broken)], capture_output=True, text=True
    )
    assert failure.returncode != 0
    assert "candidate still loses" in failure.stderr

    support_failure = subprocess.run(
        [sys.executable, str(SCRIPT), str(baseline), str(support_regressed)], capture_output=True, text=True
    )
    assert support_failure.returncode != 0
    assert "CSS support matrix changed" in support_failure.stderr

    missing_baseline = subprocess.run([sys.executable, str(SCRIPT), str(no_repro)], capture_output=True, text=True)
    assert missing_baseline.returncode != 0
    assert "baseline does not reproduce" in missing_baseline.stderr

    expected_no_repro = subprocess.run(
        [sys.executable, str(SCRIPT), "--expect-no-repro", str(no_repro)], capture_output=True, text=True
    )
    assert expected_no_repro.returncode == 0, expected_no_repro.stderr
    assert expected_no_repro.stdout.strip() == "baseline no-reproduction characterization confirmed"
    assert not expected_no_repro.stderr

    support_drift = subprocess.run(
        [sys.executable, str(SCRIPT), "--expect-no-repro", str(no_repro_support_drift)],
        capture_output=True,
        text=True,
    )
    assert support_drift.returncode != 0
    assert "baseline support matrix changed" in support_drift.stderr
    assert "mask@13" in support_drift.stderr
    assert "mask@24" in support_drift.stderr

    unexpected_repro = subprocess.run(
        [sys.executable, str(SCRIPT), "--expect-no-repro", str(baseline)], capture_output=True, text=True
    )
    assert unexpected_repro.returncode != 0
    assert "baseline unexpectedly reproduces" in unexpected_repro.stderr

    malformed = subprocess.run([sys.executable, str(SCRIPT), str(wrong_cases)], capture_output=True, text=True)
    assert malformed.returncode != 0
    assert "raster case set mismatch" in malformed.stderr
    assert "inline@24" in malformed.stderr
    assert "bogus@24" in malformed.stderr

    faint = subprocess.run(
        [sys.executable, str(SCRIPT), "--expect-no-repro", str(faint_reference)], capture_output=True, text=True
    )
    assert faint.returncode != 0
    assert "24px reference failed for inline" in faint.stderr

print("svg raster comparator tests passed")
