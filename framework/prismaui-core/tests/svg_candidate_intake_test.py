from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tests/svg_hairline/tools/prepare-ultralight-svg-sdk.py"
ARCHIVE_SIZE = 18467246


def run_intake(archive: Path, root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            str(archive),
            "--output",
            str(root / "sdk"),
            "--provenance",
            str(root / "provenance.json"),
        ],
        capture_output=True,
        text=True,
    )


with tempfile.TemporaryDirectory(prefix="prisma-svg-candidate-intake-") as temporary:
    root = Path(temporary)
    archive = root / "ultralight-free-sdk-1.4.1-dev-win-x64.7z"

    archive.write_bytes(b"not the pinned candidate archive")
    wrong_size = run_intake(archive, root)
    assert wrong_size.returncode != 0
    assert "candidate archive size mismatch" in wrong_size.stderr

    with archive.open("wb") as stream:
        stream.truncate(ARCHIVE_SIZE)
    wrong_hash = run_intake(archive, root)
    assert wrong_hash.returncode != 0
    assert "candidate archive SHA-256 mismatch" in wrong_hash.stderr

print("svg candidate intake tests passed")
