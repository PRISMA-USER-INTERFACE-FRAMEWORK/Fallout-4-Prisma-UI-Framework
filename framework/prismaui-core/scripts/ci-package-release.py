import argparse
import csv
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MATRIX = Path(os.environ.get("PRISMA_MATRIX_ROOT", ROOT.parents[1]))
PYTHON = sys.executable
HEADERS = (
    "PrismaUI_F4_API.h",
    "PrismaUI_F4_Modern_API.h",
    "PrismaUI_F4VR_API.h",
    "PrismaUI_F4_L10N.h",
)
EXPORTS = (
    "F4SEPlugin_Query",
    "F4SEPlugin_Load",
    "F4SEPlugin_Version",
    "RequestPluginAPI",
    "PrismaUI_F4_GetAPI",
    "PrismaUI_F4_GetCapabilities",
    "PrismaUI_F4_SetViewCursorPolicy",
    "PrismaUI_F4_GetViewCursorPolicy",
    "PrismaUI_F4_RegisterTranslationsV4",
)
FLAT_HEADER, MODERN_HEADER, VR_HEADER, L10N_HEADER = HEADERS
HEADER_MARKERS = (
    (FLAT_HEADER, "GetPrismaProviderModule"),
    (FLAT_HEADER, "InterfaceVersion"),
    (MODERN_HEADER, "PrismaUI_F4_GetAPI"),
    (VR_HEADER, "RequestPluginVRAPI"),
    (L10N_HEADER, "RegisterTranslationsV4"),
)
RETIRED_CEF_ARTIFACTS = ("libcef.dll", "PrismaUI_F4_CefSubprocess.exe")
SHADERS = ROOT / "assets/shaders/ultralight-1.4"
APP_CORE_LICENSE = SHADERS / "LICENSE-AppCore-LGPL-2.1.txt"
APP_CORE_NOTICE = SHADERS / "NOTICE.md"
FOMOD = ROOT / "release-assets/fomod"
TEST_MOD = ROOT / "release-assets/test-mod"
CONTROLLER_TEST_VIEWS = MATRIX / "tools/controller-test/assets/views"
CONTROLLER_TEST_FILES = (
    Path("F4SE/Plugins/PrismaControllerTest_F4.dll"),
    Path("PrismaUI_F4/views/Interface/PrismaControllerTest_F4/controller_test.html"),
    Path("PrismaUI_F4/views/Interface/PrismaControllerTest_F4/controller_test.css"),
    Path("PrismaUI_F4/views/Interface/PrismaControllerTest_F4/controller_test.js"),
)
# VR Address Library dependency declaration (source:docs/code-notes/prismaui-core.md).
VR_ADDRESS_LIBRARY_MANIFEST = MATRIX / "matrix/vr-address-library.json"
VR_ADDRESS_LIBRARY_PLUGINS = Path("Data/F4SE/plugins")
VR_ADDRESS_LIBRARY_FILE_RE = re.compile(r"^version-\d+-\d+-\d+-\d+\.(?:csv|bin)$", re.IGNORECASE)
INI_SEED = ROOT / "release-assets/PrismaUI_F4.ini"
LOCALIZATION = ROOT / "tests/localization_acceptance"
MANIFEST = ROOT / "runtime/ultralight-1.4.0.manifest.json"
SOURCE_NOTICE = """Open-source components distributed with the Ultralight 1.4 runtime

AppCore
- License: GNU LGPL 2.1
- Upstream source: https://github.com/ultralight-ux/AppCore
- PrismaUI embeds verbatim AppCore 1.4 D3D11 shader source. The exact shader source distributed
  in this package is under AppCore-1.4-shaders/ with the complete LGPL text and attribution.

WebCore / WebKit modifications
- License: GNU LGPL 2.1 and BSD components as identified by Ultralight NOTICES
- Upstream source: https://github.com/ultralight-ux/WebCore

AppCore.dll and WebCore.dll are dynamically loaded shared libraries. PrismaUI's release build
verifies the original SDK bytes when creating a release candidate, but the installed framework does
not hash-lock these LGPL shared libraries and permits interface-compatible user replacements.
"""


def run(*args: object) -> None:
    subprocess.run([str(arg) for arg in args], check=True)


def need(path: Path) -> Path:
    if not path.is_file():
        raise FileNotFoundError(f"missing build artifact: {path}")
    return path


def copy(source: Path, destination: Path) -> None:
    need(source)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def same(source: Path, destination: Path) -> None:
    if source.read_bytes() != destination.read_bytes():
        raise ValueError(f"package file differs from source: {destination}")


# Address-library coverage rule; source:docs/code-notes/prismaui-core.md.
def _release_version(value: object, what: str) -> tuple[int, int, int]:
    match = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)", str(value or ""))
    if not match:
        raise ValueError(f"{what} is not an X.Y.Z release version: {value!r}")
    return tuple(int(part) for part in match.groups())


def load_vr_address_library_manifest(path: Path = VR_ADDRESS_LIBRARY_MANIFEST) -> dict:
    manifest = json.loads(need(path).read_text(encoding="utf-8"))
    if manifest.get("schema_version") != 1:
        raise ValueError(f"VR address library manifest schema_version must be 1: {path}")
    dependency = manifest.get("dependency")
    minimum = manifest.get("minimum")
    required = manifest.get("required_ids")
    if not isinstance(dependency, dict) or not isinstance(minimum, dict) or not isinstance(required, list):
        raise ValueError(f"VR address library manifest is missing dependency/minimum/required_ids: {path}")
    if dependency.get("id") != "vr-address-library" or dependency.get("nexus_mod_id") != 64879:
        raise ValueError("VR address library manifest must declare Nexus mod 64879 as vr-address-library")
    if dependency.get("distributed_by_prisma") is not False:
        raise ValueError("VR address library manifest must declare that PrismaUI does not distribute the file")
    runtime = str(dependency.get("runtime") or "")
    if not re.fullmatch(r"\d+\.\d+\.\d+\.\d+", runtime):
        raise ValueError(f"VR address library manifest runtime is not a four-part version: {runtime!r}")
    install_file = f"Data/F4SE/plugins/version-{runtime.replace('.', '-')}.csv"
    if dependency.get("install_file") != install_file:
        raise ValueError(f"VR address library manifest install_file must be {install_file}")
    minimum_version = _release_version(minimum.get("version"), "VR address library minimum version")
    entries = minimum.get("entries")
    if not isinstance(entries, int) or isinstance(entries, bool) or entries <= 0:
        raise ValueError("VR address library minimum entries must be a positive integer")
    if not re.fullmatch(r"[0-9a-f]{64}", str(minimum.get("sha256") or "").lower()):
        raise ValueError("VR address library minimum sha256 must be 64 hex characters")
    if not required:
        raise ValueError("VR address library manifest lists no required ids")
    ids = set()
    firsts = []
    for row in required:
        ident = row.get("id") if isinstance(row, dict) else None
        if not isinstance(ident, int) or isinstance(ident, bool) or ident <= 0:
            raise ValueError(f"VR address library required id is not a positive integer: {row!r}")
        if ident in ids:
            raise ValueError(f"duplicate VR address library required id {ident}")
        ids.add(ident)
        firsts.append(_release_version(row.get("first_available"), f"required id {ident} first_available"))
    if minimum_version != max(firsts):
        raise ValueError(
            "VR address library minimum version must equal the newest required id first_available; "
            "declare the newer minimum deliberately when a required id needs it")
    return {
        "version": minimum_version,
        "entries": entries,
        "required_ids": frozenset(ids),
        "install_file": install_file,
    }


def parse_vr_address_library_csv(path: Path) -> tuple[tuple[int, int, int], int, set[int]]:
    rows = list(csv.reader(need(path).read_text(encoding="utf-8-sig").splitlines()))
    if len(rows) < 3 or [cell.strip() for cell in rows[0][:2]] != ["id", "offset"] or len(rows[1]) < 2:
        raise ValueError(f"VR Address Library CSV has no valid header and metadata row: {path}")
    count, version = (cell.strip() for cell in rows[1][:2])
    if not count.isdigit() or int(count) <= 0:
        raise ValueError(f"VR Address Library CSV metadata row has no positive entry count: {rows[1]}")
    release = _release_version(version, f"VR Address Library CSV metadata version in {path.name}")
    ids = set()
    # Blank rows must be skipped; source:docs/code-notes/prismaui-core.md.
    for row in rows[2:]:
        if not row or all(not cell.strip() for cell in row):
            continue
        if len(row) < 2:
            raise ValueError(f"VR Address Library CSV has a short data row: {row}")
        ident, offset = (cell.strip() for cell in row[:2])
        if not ident.isdigit() or not re.fullmatch(r"[0-9A-Fa-f]{1,16}", offset):
            raise ValueError(f"VR Address Library CSV has a malformed data row: {row}")
        if int(ident) in ids:
            raise ValueError(f"VR Address Library CSV has duplicate id {ident}: {path}")
        ids.add(int(ident))
    if int(count) != len(ids):
        raise ValueError(f"VR Address Library CSV metadata count {count} does not match {len(ids)} data rows")
    return release, int(count), ids


def verify_vr_address_library_coverage(csv_path: Path, manifest: dict) -> int:
    release, entries, ids = parse_vr_address_library_csv(csv_path)
    installed = ".".join(str(part) for part in release)
    declared = ".".join(str(part) for part in manifest["version"])
    if release < manifest["version"]:
        raise ValueError(
            f"installed VR Address Library {installed} is older than the supported minimum {declared}; "
            f"install the official Nexus 64879 release {declared} or newer")
    if entries < manifest["entries"]:
        raise ValueError(
            f"installed VR Address Library {installed} has {entries} entries, below the {declared} minimum")
    missing = sorted(manifest["required_ids"] - ids)
    if missing:
        raise ValueError(f"installed VR Address Library {installed} is missing required ids: {missing}")
    return entries


def check_no_shared_address_library(stage: Path, label: str) -> None:
    plugins = stage / VR_ADDRESS_LIBRARY_PLUGINS
    for path in sorted(stage.rglob("*")):
        if not path.is_file():
            continue
        if VR_ADDRESS_LIBRARY_FILE_RE.fullmatch(path.name) or (
                path.parent == plugins and path.suffix.lower() in (".csv", ".bin")):
            raise ValueError(
                f"{label} package must not contain a shared Address Library file: {path.relative_to(stage)}")


def controller_test_sources(flat_build: Path) -> list[tuple[Path, Path]]:
    dll, *views = CONTROLLER_TEST_FILES
    return [(flat_build / dll.name, dll), *((CONTROLLER_TEST_VIEWS / view.name, view) for view in views)]


def stage_controller_test(stage: Path, flat_build: Path) -> None:
    for source, relative in controller_test_sources(flat_build):
        copy(source, stage / "ControllerTestMod" / relative)


def check_controller_test(stage: Path, flat: bool, flat_build: Path) -> None:
    payload = stage / "ControllerTestMod"
    if not flat:
        if payload.exists():
            raise ValueError("VR package contains the flat-only controller test mod")
        return
    for source, relative in controller_test_sources(flat_build):
        same(source, need(payload / relative))


def configured_version() -> str:
    match = re.search(r'^\s*local PRISMA_VERSION = "(\d+\.\d+\.\d+)"',
                      (ROOT / "xmake.lua").read_text(encoding="utf-8"), re.MULTILINE)
    if not match:
        raise ValueError("could not resolve PRISMA_VERSION from xmake.lua")
    return match.group(1)


def require_ini_default(path: Path) -> None:
    if "bMeshBinding=0" not in path.read_text(encoding="utf-8").splitlines():
        raise ValueError("release INI must disable MeshBinding by default")


def archive(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(destination, "w", zipfile.ZIP_DEFLATED) as output:
        for path in sorted(source.rglob("*")):
            if path.is_file():
                output.write(path, path.relative_to(source).as_posix())
    print(f"wrote {destination} {destination.stat().st_size}")


def check_exports(path: Path, vr: bool) -> None:
    command: list[object] = [PYTHON, MATRIX / ".github/scripts/validate-pe-x64.py", path]
    for name in EXPORTS:
        command.extend(("--require-export", name))
    if not vr:
        command.extend(("--require-export", "PrismaUI_F4_GetRuntimeState"))
    command.extend(("--require-export" if vr else "--forbid-export", "RequestPluginVRAPI"))
    run(*command)


def stage_ultralight_runtime(package: Path) -> None:
    args: list[object] = [PYTHON, ROOT / "scripts/stage-ultralight-runtime.py",
                          "--package-root", package, "--manifest", MANIFEST]
    if os.environ.get("ULTRALIGHT_RUNTIME_DIR"):
        args.extend(("--source", os.environ["ULTRALIGHT_RUNTIME_DIR"]))
    elif os.environ.get("ULTRALIGHT_SDK_ARCHIVE"):
        args.extend(("--archive", os.environ["ULTRALIGHT_SDK_ARCHIVE"]))
    run(*args)


def stage_appcore_oss(package: Path) -> None:
    legal = package / "licenses"
    copy(APP_CORE_LICENSE, legal / "AppCore-LGPL-2.1.txt")
    copy(APP_CORE_NOTICE, legal / "AppCore-NOTICE.md")
    for kind in ("vs", "ps"):
        for shader in sorted((SHADERS / kind).glob("*.hlsl")):
            copy(shader, legal / "AppCore-1.4-shaders" / kind / shader.name)
    (legal / "AppCore-WebCore-SOURCE.txt").write_text(SOURCE_NOTICE, encoding="utf-8", newline="\n")


def stage_common(stage: Path, version: str, vr: bool, flat_build: Path) -> None:
    fomod = stage / "fomod"
    fomod.mkdir(parents=True)
    module = "ModuleConfigVR.xml" if vr else "ModuleConfig.xml"
    for name, source in (("info.xml", "info.xml"), ("ModuleConfig.xml", module)):
        (fomod / name).write_bytes(need(FOMOD / source).read_bytes().replace(b"@VERSION@", version.encode("ascii")))
    copy(FOMOD / "screenshot.png", fomod / "screenshot.png")
    if not vr:
        shutil.copytree(TEST_MOD, stage / "TestMod")
        stage_controller_test(stage, flat_build)
        local_mod = stage / "LocalizationTestMod"
        copy(flat_build / "PrismaLocalizationAcceptance.dll",
             local_mod / "F4SE/Plugins/PrismaLocalizationAcceptance.dll")
        copy(LOCALIZATION / "assets/views/Interface/PrismaLocalizationAcceptance/test.html",
             local_mod / "PrismaUI_F4/views/Interface/PrismaLocalizationAcceptance/test.html")
        for language in ("en", "fr"):
            filename = f"PrismaLocalizationAcceptance_{language}.json"
            copy(LOCALIZATION / "fixtures/Loose/Data/Interface/Translations" / filename,
                 local_mod / "Interface/Translations" / filename)

    plugins = stage / "Data/F4SE/plugins"
    copy(INI_SEED, plugins / "PrismaUI_F4.ini")
    require_ini_default(plugins / "PrismaUI_F4.ini")
    package = stage / "Data/PrismaUI_F4"
    if package.exists():
        shutil.rmtree(package)
    (package / "views").mkdir(parents=True)
    if (ROOT / "assets/views").is_dir():
        shutil.copytree(ROOT / "assets/views", package / "views", dirs_exist_ok=True)
    inspector = package / "views/inspector"
    if inspector.is_dir():
        shutil.copytree(inspector, package / "inspector", dirs_exist_ok=True)
        shutil.rmtree(inspector)
    if (ROOT / "assets/misc").is_dir():
        shutil.copytree(ROOT / "assets/misc", package / "misc")
    stage_ultralight_runtime(package)
    stage_appcore_oss(package)
    copy(MANIFEST, package / "runtime" / MANIFEST.name)

    copy(ROOT / "scripts/PrismaUI.pex", stage / "Data/Scripts/PrismaUI.pex")
    for script in sorted((ROOT / "scripts/Source").glob("*.psc")):
        copy(script, stage / "Data/Scripts/Source" / script.name)
    for header in HEADERS:
        copy(ROOT / "src" / header, stage / "Developer" / header)


def check_developer_headers(stage: Path) -> None:
    developer = stage / "Developer"
    shipped = sorted(path.name for path in developer.iterdir())
    if shipped != sorted(HEADERS):
        raise ValueError(f"Developer/ must ship exactly {sorted(HEADERS)}, found {shipped}")
    for header, marker in HEADER_MARKERS:
        if marker not in (developer / header).read_text(encoding="utf-8"):
            raise ValueError(f"Developer/{header} is missing {marker}")


def verify_zip(path: Path, destination: Path) -> None:
    result = subprocess.run([PYTHON, str(ROOT / "scripts/stage-ultralight-runtime.py"),
                             "--verify-release-archive", str(path)])
    if result.returncode != 0:
        raise ValueError(
            "production archive contains retired CEF artifacts "
            f"({' or '.join(RETIRED_CEF_ARTIFACTS)}) or an invalid legal payload: {path}"
        )
    if destination.exists():
        shutil.rmtree(destination)
    with zipfile.ZipFile(path) as source:
        source.extractall(destination)
    package = destination / "Data/PrismaUI_F4"
    run(PYTHON, ROOT / "scripts/validate-ultralight-runtime.py", package, "--manifest", MANIFEST)
    legal = package / "licenses"
    same(ROOT / "assets/misc/cursor.png", package / "misc/cursor.png")
    same(APP_CORE_LICENSE, legal / "AppCore-LGPL-2.1.txt")
    same(APP_CORE_NOTICE, legal / "AppCore-NOTICE.md")
    for kind, name in (("vs", "v2f_c4f_t2f.hlsl"), ("vs", "v2f_c4f_t2f_t2f_d28f.hlsl"),
                       ("ps", "fill.hlsl"), ("ps", "fill_path.hlsl")):
        same(SHADERS / kind / name, legal / "AppCore-1.4-shaders" / kind / name)
    if "AppCore.dll and WebCore.dll are dynamically loaded shared libraries" not in (
        legal / "AppCore-WebCore-SOURCE.txt"
    ).read_text(encoding="utf-8"):
        raise ValueError("package is missing AppCore/WebCore source notice")


def main() -> None:
    # VR address coverage rule source:docs/code-notes/prismaui-core.md.
    parser = argparse.ArgumentParser(description="Assemble the PrismaUI_F4 release zips from a native clang-cl build.")
    parser.add_argument("version")
    parser.add_argument("output", type=Path, nargs="?", default=ROOT / "dist-release")
    parser.add_argument(
        "--require-vr-address-library",
        action="store_true",
        help="fail unless F4VR_ADDRESS_LIBRARY_CSV names a VR Address Library that covers the declared minimum")
    args = parser.parse_args()
    version = args.version
    vr_manifest = load_vr_address_library_manifest()
    configured_csv = os.environ.get("F4VR_ADDRESS_LIBRARY_CSV")
    if args.require_vr_address_library and not configured_csv:
        raise SystemExit("--require-vr-address-library needs F4VR_ADDRESS_LIBRARY_CSV")
    if configured_csv:
        entries = verify_vr_address_library_coverage(Path(configured_csv), vr_manifest)
        declared = ".".join(str(part) for part in vr_manifest["version"])
        print(f"VR Address Library coverage check passed: {entries} entries, declared minimum {declared}")
    configured = configured_version()
    if version != configured:
        raise SystemExit(f"package version {version} does not match compiled PRISMA_VERSION {configured}")

    flat = Path(os.environ.get("PRISMA_BUILD_FLAT", MATRIX / "build-xwin-fw")) / "windows/x64/release"
    vr = Path(os.environ.get("PRISMA_BUILD_VR", MATRIX / "build-xwin-vr")) / "windows/x64/release"
    commit = subprocess.check_output(["git", "-C", str(ROOT), "rev-parse", "HEAD"], text=True).strip()
    run(PYTHON, ROOT / "scripts/build-provenance.py", "verify", "--flat", flat, "--vr", vr,
        "--version", version, "--commit", commit)

    for path in (
        flat / "PrismaUI_F4.dll", flat / "PrismaUI_F4.pdb", flat / "PrismaLocalizationAcceptance.dll",
        flat / "PrismaControllerTest_F4.dll", vr / "PrismaUI_F4VR.dll", vr / "PrismaUI_F4VR.pdb",
        *(ROOT / "src" / header for header in HEADERS),
        ROOT / "scripts/PrismaUI.pex", MANIFEST, ROOT / "assets/misc/cursor.png",
        APP_CORE_LICENSE, APP_CORE_NOTICE, INI_SEED,
        FOMOD / "info.xml", FOMOD / "ModuleConfig.xml", FOMOD / "ModuleConfigVR.xml", FOMOD / "screenshot.png",
        TEST_MOD / "F4SE/Plugins/PrismaUI_SVG_Test.dll",
        TEST_MOD / "PrismaUI_F4/views/PrismaSVGTest/hairline.svg",
        TEST_MOD / "PrismaUI_F4/views/PrismaSVGTest/svg.html",
        *(source for source, _ in controller_test_sources(flat)),
        LOCALIZATION / "assets/views/Interface/PrismaLocalizationAcceptance/test.html",
        LOCALIZATION / "fixtures/Loose/Data/Interface/Translations/PrismaLocalizationAcceptance_en.json",
        LOCALIZATION / "fixtures/Loose/Data/Interface/Translations/PrismaLocalizationAcceptance_fr.json",
    ):
        need(path)
    require_ini_default(INI_SEED)

    output = args.output
    output.mkdir(parents=True, exist_ok=True)
    packages = []
    with tempfile.TemporaryDirectory(prefix="prisma-release-") as temporary:
        work = Path(temporary)
        stages = {}
        for suffix, is_vr, build, dll in (("OG-AE", False, flat, "PrismaUI_F4.dll"),
                                          ("VR", True, vr, "PrismaUI_F4VR.dll")):
            stage = work / suffix
            stage_common(stage, version, is_vr, flat)
            provider = stage / "Data/F4SE/plugins" / dll
            copy(build / dll, provider)
            check_exports(provider, is_vr)
            check_controller_test(stage, not is_vr, flat)
            check_no_shared_address_library(stage, suffix)
            stages[suffix] = stage
        if (stages["OG-AE"] / "Data/F4SE/plugins/PrismaUI_F4VR.dll").exists():
            raise ValueError("OG-AE package contains the VR provider")
        if (stages["VR"] / "Data/F4SE/plugins/PrismaUI_F4.dll").exists():
            raise ValueError("VR package contains the flat provider")
        check_developer_headers(stages["OG-AE"])
        for suffix, stage in stages.items():
            same(ROOT / "assets/misc/cursor.png", stage / "Data/PrismaUI_F4/misc/cursor.png")
            package_path = output / f"PrismaUI_F4-{version}-Fallout4-{suffix}.zip"
            archive(stage, package_path)
            packages.append(package_path)

        symbols = output / f"PrismaUI_F4-{version}-symbols.zip"
        with zipfile.ZipFile(symbols, "w", zipfile.ZIP_DEFLATED) as zipped:
            zipped.write(flat / "PrismaUI_F4.pdb", "PrismaUI_F4.pdb")
            zipped.write(vr / "PrismaUI_F4VR.pdb", "PrismaUI_F4VR.pdb")
        print(f"wrote {symbols} {symbols.stat().st_size}")

        for package_path, suffix in zip(packages, stages):
            verify_zip(package_path, work / f"verify-{suffix}")
            check_controller_test(work / f"verify-{suffix}", suffix == "OG-AE", flat)
            check_no_shared_address_library(work / f"verify-{suffix}", f"extracted {suffix}")

    for path in (flat / "PrismaUI_F4.dll", flat / "PrismaUI_F4.pdb", flat / "PrismaLocalizationAcceptance.dll",
                 flat / "PrismaControllerTest_F4.dll", vr / "PrismaUI_F4VR.dll", vr / "PrismaUI_F4VR.pdb",
                 *packages, symbols):
        print(f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path}")
    print(f"packages ready in {output}")


if __name__ == "__main__":
    main()

