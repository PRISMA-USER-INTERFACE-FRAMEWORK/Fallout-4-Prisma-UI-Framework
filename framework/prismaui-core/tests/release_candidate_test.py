import importlib.util
import json
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "release_candidate", ROOT / "scripts" / "release-candidate.py"
)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(MODULE)


def write_zip(path: Path, members: dict[str, bytes]) -> None:
    with zipfile.ZipFile(path, "w") as archive:
        for member, payload in members.items():
            archive.writestr(member, payload)


def desktop_members(dll: bytes, acceptance: bytes) -> dict[str, bytes]:
    return {
        MODULE.DLL_MEMBERS["PrismaUI_F4.dll"]: dll,
        MODULE.ACCEPTANCE_MEMBER: acceptance,
    }


def write_symbols(path: Path) -> None:
    write_zip(
        path,
        {
            MODULE.PDB_MEMBERS["PrismaUI_F4.pdb"]: b"desktop-pdb",
            MODULE.PDB_MEMBERS["PrismaUI_F4VR.pdb"]: b"vr-pdb",
        },
    )


with tempfile.TemporaryDirectory() as temporary:
    directory = Path(temporary)
    version = "2.1.0"
    commit = "a" * 40
    desktop = directory / f"PrismaUI_F4-{version}-Fallout4-OG-AE.zip"
    vr = directory / f"PrismaUI_F4-{version}-Fallout4-VR.zip"
    symbols = directory / f"PrismaUI_F4-{version}-symbols.zip"
    write_zip(desktop, desktop_members(b"desktop-dll", b"acceptance-dll"))
    write_zip(vr, {MODULE.DLL_MEMBERS["PrismaUI_F4VR.dll"]: b"vr-dll"})
    write_symbols(symbols)

    MODULE.create(directory, version, commit, "https://example.invalid/run/1")
    sums = MODULE.read_sums(directory / "SHA256SUMS.txt")
    assert "PrismaLocalizationAcceptance.dll" in sums
    MODULE.verify(directory, version, commit, sums["PrismaUI_F4.dll"], sums[desktop.name])

    write_zip(desktop, {MODULE.DLL_MEMBERS["PrismaUI_F4.dll"]: b"desktop-dll"})
    try:
        MODULE.create(directory, version, commit, "https://example.invalid/run/1")
    except KeyError:
        pass
    else:
        raise AssertionError("a flat package without the acceptance DLL was accepted")
    write_zip(desktop, desktop_members(b"desktop-dll", b"acceptance-dll"))
    MODULE.create(directory, version, commit, "https://example.invalid/run/1")
    sums = MODULE.read_sums(directory / "SHA256SUMS.txt")
    MODULE.verify(directory, version, commit, sums["PrismaUI_F4.dll"], sums[desktop.name])

    for invalid in ("", "g" * 64, "a" * 63):
        for dll, archive in ((invalid, None), (None, invalid)):
            try:
                MODULE.verify(directory, version, commit, dll, archive)
            except ValueError:
                pass
            else:
                raise AssertionError("explicitly supplied invalid acceptance hash was ignored")
    for invalid in ("", "main", "a" * 39, "g" * 40):
        try:
            MODULE.create(directory, version, invalid, "https://example.invalid/run/1")
        except ValueError:
            pass
        else:
            raise AssertionError("candidate accepted an invalid source commit")

    vr.unlink()
    MODULE.verify(directory, version, commit, sums["PrismaUI_F4.dll"], sums[desktop.name])

    write_zip(desktop, desktop_members(b"changed", b"acceptance-dll"))
    try:
        MODULE.verify(directory, version, commit, None, None)
    except ValueError:
        pass
    else:
        raise AssertionError("changed release-candidate bytes must fail verification")

    write_zip(desktop, desktop_members(b"desktop-dll", b"acceptance-dll"))
    write_symbols(symbols)
    with zipfile.ZipFile(symbols, "a") as archive:
        archive.writestr("unexpected.txt", b"changed-symbols")
    try:
        MODULE.verify(directory, version, commit, None, None)
    except ValueError:
        pass
    else:
        raise AssertionError("changed release symbols must fail verification")

    record = json.loads((directory / "RELEASE-CANDIDATE.json").read_text(encoding="utf-8"))
    assert record["schema_version"] == 2
    assert record["version"] == version and record["commit"] == commit
    assert "PrismaUI_F4.pdb" in record["sha256"]
    assert "PrismaLocalizationAcceptance.dll" in record["sha256"]
    assert symbols.name in record["sha256"]

print("release candidate create/verify tests passed")
