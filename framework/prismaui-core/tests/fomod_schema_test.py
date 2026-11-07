from __future__ import annotations

import argparse
import hashlib
import io
import json
import shutil
import tempfile
import urllib.request
from pathlib import Path
from xml.etree import ElementTree


ROOT = Path(__file__).resolve().parents[1]
FOMOD = ROOT / "release-assets" / "fomod"
TEST_MOD = ROOT / "release-assets" / "test-mod"
TEST_MOD_PROVENANCE = ROOT / "tests" / "svg_test_mod_provenance.json"
TEST_MOD_FILES = (
    Path("F4SE/Plugins/PrismaUI_SVG_Test.dll"),
    Path("PrismaUI_F4/views/PrismaSVGTest/svg.html"),
    Path("PrismaUI_F4/views/PrismaSVGTest/hairline.svg"),
)
LOCALIZATION_TEST_SOURCE = ROOT / "tests" / "localization_acceptance"
CONTROLLER_TEST_SOURCE = ROOT.parents[1] / "tools" / "controller-test"
CONTROLLER_TEST_FILES = (
    Path("F4SE/Plugins/PrismaControllerTest_F4.dll"),
    Path("PrismaUI_F4/views/Interface/PrismaControllerTest_F4/controller_test.html"),
    Path("PrismaUI_F4/views/Interface/PrismaControllerTest_F4/controller_test.css"),
    Path("PrismaUI_F4/views/Interface/PrismaControllerTest_F4/controller_test.js"),
)
LOCALIZATION_TEST_FILES = (
    Path("PrismaUI_F4/views/Interface/PrismaLocalizationAcceptance/test.html"),
    Path("Interface/Translations/PrismaLocalizationAcceptance_en.json"),
    Path("Interface/Translations/PrismaLocalizationAcceptance_fr.json"),
)
SCHEMA_URL = (
    "https://raw.githubusercontent.com/Cutleast/FOMOD-Creation-Kit/"
    "a32ef403e6787c156a40d018f5cc1a29e97a4ef4/res/schemas/ModConfig5.0.xsd"
)


def render(path: Path, version: str) -> ElementTree.Element:
    text = path.read_text(encoding="utf-8").replace("@VERSION@", version)
    assert "@VERSION@" not in text
    return ElementTree.fromstring(text)


def validate_templates(fomod: Path) -> None:
    for module_name, expected_test_mod in (("ModuleConfig.xml", True), ("ModuleConfigVR.xml", False)):
        for version in ("2.1.0", "2.1.1"):
            info = render(fomod / "info.xml", version)
            module = render(fomod / module_name, version)
            validate_fomod(info, module, version, fomod, expected_test_mod)
    validate_test_mod(TEST_MOD)
    validate_localization_test_source()
    validate_controller_test_source()


def validate_localization_test_source() -> None:
    source_files = (
        LOCALIZATION_TEST_SOURCE / "assets/views/Interface/PrismaLocalizationAcceptance/test.html",
        LOCALIZATION_TEST_SOURCE / "fixtures/Loose/Data/Interface/Translations/PrismaLocalizationAcceptance_en.json",
        LOCALIZATION_TEST_SOURCE / "fixtures/Loose/Data/Interface/Translations/PrismaLocalizationAcceptance_fr.json",
    )
    for path in source_files:
        assert path.is_file(), path


def validate_controller_test_source() -> None:
    assert (CONTROLLER_TEST_SOURCE / "xmake.lua").is_file()
    for relative in CONTROLLER_TEST_FILES[1:]:
        assert (CONTROLLER_TEST_SOURCE / "assets/views" / relative.name).is_file(), relative


def validate_controller_test_mod(payload_root: Path) -> None:
    for relative in CONTROLLER_TEST_FILES:
        assert (payload_root / relative).is_file(), relative


def validate_test_mod(payload_root: Path) -> None:
    provenance = json.loads(TEST_MOD_PROVENANCE.read_text(encoding="utf-8"))
    for relative in TEST_MOD_FILES:
        assert (payload_root / relative).is_file(), relative
    dll = payload_root / TEST_MOD_FILES[0]
    assert dll.stat().st_size == provenance["size"]
    assert hashlib.sha256(dll.read_bytes()).hexdigest() == provenance["sha256"]
    blob = f"blob {dll.stat().st_size}\0".encode() + dll.read_bytes()
    assert hashlib.sha1(blob).hexdigest() == provenance["git_blob_sha1"]


def validate_no_test_mod(payload_root: Path) -> None:
    assert not payload_root.exists()


def validate_localization_test_mod(payload_root: Path) -> None:
    assert (payload_root / "F4SE/Plugins/PrismaLocalizationAcceptance.dll").is_file()
    for relative in LOCALIZATION_TEST_FILES:
        assert (payload_root / relative).is_file(), relative


def validate_fomod(
    info: ElementTree.Element,
    module: ElementTree.Element,
    version: str,
    fomod: Path,
    expected_test_mod: bool,
) -> None:
    assert info.findtext("Version") == version
    assert info.find("Version").attrib["MachineVersion"] == version
    assert module.findtext("moduleName") == f"PrismaUI F4 {version}"
    plugins = module.findall("./installSteps/installStep/optionalFileGroups/group/plugins/plugin")
    expected_types = ["Required", "Optional"] + (["Optional", "Optional", "Optional"] if expected_test_mod else [])
    assert [plugin.find("typeDescriptor/type").attrib["name"] for plugin in plugins] == expected_types
    test_plugins = [plugin for plugin in plugins if plugin.attrib.get("name") == "Install SVG raster test mod"]
    localization_plugins = [plugin for plugin in plugins if plugin.attrib.get("name") == "Install V4 localization test mod"]
    controller_plugins = [plugin for plugin in plugins if plugin.attrib.get("name") == "Install controller test mod"]
    assert bool(test_plugins) == expected_test_mod
    assert bool(localization_plugins) == expected_test_mod
    assert bool(controller_plugins) == expected_test_mod
    if expected_test_mod:
        assert test_plugins[0].find("files/folder").attrib["source"] == "TestMod"
        assert localization_plugins[0].find("files/folder").attrib["source"] == "LocalizationTestMod"
        assert controller_plugins[0].find("files/folder").attrib["source"] == "ControllerTestMod"
    assert (fomod / "screenshot.png").is_file()
    support = plugins[0].findtext("description")
    policy = (
        ("NG 1.10.984 supported (static RE; not yet live-tested)", "NG versions other than 1.10.984 are unsupported")
        if expected_test_mod else ("1.10.980-1.10.984 is unsupported",)
    )
    for required in (
        "OG 1.10.163", "AE 1.11.137 onward", "later 1.x minor versions",
        "matching F4SE and Address Library support", *policy,
        "Compatibility with future executables remains unverified", "separate VR archive",
    ):
        assert required in support, f"installer support policy is missing: {required}"


def validate_schema(fomod: Path, required: bool, module_name: str) -> None:
    try:
        from lxml import etree
    except ImportError:
        if required:
            raise
        print("FOMOD XSD validation deferred: lxml is not installed")
        return
    with urllib.request.urlopen(SCHEMA_URL, timeout=30) as response:
        schema = etree.XMLSchema(etree.parse(io.BytesIO(response.read())))
    document = etree.parse(str(fomod / module_name))
    assert schema.validate(document), str(schema.error_log)
    print(f"FOMOD {module_name} validates against ModConfig5.0.xsd")


def validate_missing_test_file_fails(package_root: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="prisma-fomod-test-mod-") as temp:
        root = Path(temp)
        shutil.copytree(package_root / "fomod", root / "fomod")
        shutil.copytree(package_root / "TestMod", root / "TestMod")
        shutil.copytree(package_root / "LocalizationTestMod", root / "LocalizationTestMod")
        shutil.copytree(package_root / "Data/F4SE/plugins", root / "Data/F4SE/plugins")
        (root / "LocalizationTestMod" / LOCALIZATION_TEST_FILES[0]).unlink()
        info = ElementTree.parse(root / "fomod/info.xml").getroot()
        module = ElementTree.parse(root / "fomod/ModuleConfig.xml").getroot()
        try:
            validate_fomod(info, module, info.findtext("Version"), root / "fomod", True)
            validate_localization_test_mod(root / "LocalizationTestMod")
        except AssertionError:
            return
        raise AssertionError("packaged TestMod missing-file regression was not detected")


parser = argparse.ArgumentParser()
parser.add_argument("--require-xsd", action="store_true")
parser.add_argument("--root", type=Path)
args = parser.parse_args()
fomod = FOMOD if args.root is None else args.root / "fomod"
if args.root is None:
    validate_templates(fomod)
    validate_schema(fomod, args.require_xsd, "ModuleConfig.xml")
    validate_schema(fomod, args.require_xsd, "ModuleConfigVR.xml")
else:
    assert "@VERSION@" not in (fomod / "info.xml").read_text(encoding="utf-8")
    assert "@VERSION@" not in (fomod / "ModuleConfig.xml").read_text(encoding="utf-8")
    info = ElementTree.parse(fomod / "info.xml").getroot()
    version = info.findtext("Version")
    assert version
    package_root = args.root
    flat = (package_root / "Data/F4SE/plugins/PrismaUI_F4.dll").is_file()
    vr = (package_root / "Data/F4SE/plugins/PrismaUI_F4VR.dll").is_file()
    assert flat != vr, "package must identify exactly one provider target"
    expected_test_mod = flat
    validate_fomod(
        info,
        ElementTree.parse(fomod / "ModuleConfig.xml").getroot(),
        version,
        fomod,
        expected_test_mod,
    )
    if expected_test_mod:
        validate_test_mod(package_root / "TestMod")
        validate_localization_test_mod(package_root / "LocalizationTestMod")
        validate_controller_test_mod(package_root / "ControllerTestMod")
        validate_missing_test_file_fails(package_root)
    else:
        validate_no_test_mod(package_root / "TestMod")
        validate_no_test_mod(package_root / "LocalizationTestMod")
        validate_no_test_mod(package_root / "ControllerTestMod")
validate_schema(fomod, args.require_xsd, "ModuleConfig.xml")
print("FOMOD template contract passed")
