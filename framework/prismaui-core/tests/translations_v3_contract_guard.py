from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
API = (ROOT / "src/API/API.cpp").read_text(encoding="utf-8")
API_HEADER = (ROOT / "src/PrismaUI_F4_API.h").read_text(encoding="utf-8")
API_CLASS = (ROOT / "src/API/API.h").read_text(encoding="utf-8")
TRANSLATIONS = (ROOT / "src/PrismaUI/Translations.cpp").read_text(encoding="utf-8")
TRANSLATIONS_LANGUAGE = (ROOT / "src/PrismaUI/TranslationsLanguage.cpp").read_text(encoding="utf-8")
TRANSLATIONS_SCRIPT = (ROOT / "src/PrismaUI/TranslationsScript.cpp").read_text(encoding="utf-8")
CI_GATE = (ROOT / "tests/ci_windows_gate.py").read_text(encoding="utf-8")
TRANSLATIONS_HEADER = (ROOT / "src/PrismaUI/Translations.h").read_text(encoding="utf-8")
TRANSLATIONS_TEXT = (ROOT / "src/PrismaUI/TranslationsText.cpp").read_text(encoding="utf-8")
TRANSLATIONS_JSON = (ROOT / "src/PrismaUI/TranslationsJson.cpp").read_text(encoding="utf-8")
WEB_RUNTIME = (ROOT / "src/PrismaUI/WebRuntimeCallbacks.cpp").read_text(encoding="utf-8")
WEB_RUNTIME_HEADER = (ROOT / "src/PrismaUI/WebRuntime.h").read_text(encoding="utf-8")
BACKEND_API = (ROOT / "src/PrismaUI/IWebBackend.h").read_text(encoding="utf-8")
BACKEND = (ROOT / "src/PrismaUI/UltralightBackendInput.cpp").read_text(encoding="utf-8")
VIEW_MANAGER = (ROOT / "src/PrismaUI/UltralightViewManager.h").read_text(encoding="utf-8")
DOCUMENT_CALLBACKS = (ROOT / "src/PrismaUI/UltralightDocumentCallbacks.cpp").read_text(encoding="utf-8")
PUBLIC_L10N = (ROOT / "src/PrismaUI_F4_L10N.h").read_text(encoding="utf-8")
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
VR_MAIN = (ROOT / "src/VR/VRMain.cpp").read_text(encoding="utf-8")
EXAMPLE_HEADER = (ROOT / "example-f4se-plugin/src/PrismaUI_F4_API.h").read_text(encoding="utf-8")
ACCEPTANCE_MAIN = (ROOT / "tests/localization_acceptance/src/main.cpp").read_text(encoding="utf-8")
ACCEPTANCE_XMAKE = (ROOT / "tests/localization_acceptance/xmake.lua").read_text(encoding="utf-8")
ACCEPTANCE_PAGE = (ROOT / "tests/localization_acceptance/assets/views/Interface/PrismaLocalizationAcceptance/test.html").read_text(encoding="utf-8")
FALLBACK_BA2 = (ROOT / "tests/localization_acceptance/fixtures/FallbackBA2/Data/Interface/Translations/PrismaLocalizationAcceptance_en.json").read_text(encoding="utf-8")
FRAMEWORK_XMAKE = (ROOT.parent / "xmake.lua").read_text(encoding="utf-8")


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError(f"unclosed function: {signature}")


def assert_order(source: str, *needles: str) -> None:
    positions = [source.index(needle) for needle in needles]
    assert positions == sorted(positions), needles


assert "class IVPrismaUI3 : public IVPrismaUI2" in API_HEADER
assert "virtual void RegisterTranslations(PrismaView view, const char* pluginName) noexcept = 0;" in API_HEADER
assert "virtual void RegisterTranslations(PrismaView view, const char* pluginName) noexcept override;" in API_CLASS
assert "virtual void RegisterTranslations(PrismaView view, const char* pluginName) noexcept = 0;" in EXAMPLE_HEADER
assert "std::unordered_map<std::string, std::string> ParseTranslationFile" in TRANSLATIONS_HEADER
assert "std::map<std::string, std::string> ParseJsonTranslationFile" in TRANSLATIONS_HEADER
assert '#include "TranslationsScript.h"' in TRANSLATIONS_HEADER

register = function_body(API, "void PluginAPI::PrismaUIInterface::RegisterTranslations(")
assert "IsFrameworkSystemView(view)" in register
assert "!pluginName || pluginName[0] == '\\0'" in register
assert_order(
    register,
    "DetectGameLanguage()",
    "ParseTranslationFile(plugin, lang)",
    "entries.empty()",
    "BuildL10NScript(entries)",
    "WebRuntime::Invoke(view, script, nullptr)",
)
assert '[V3] RegisterTranslations: no translations loaded' in register

detect = function_body(TRANSLATIONS, "std::string DetectGameLanguage()")
assert_order(
    detect,
    "LanguageIniCandidates(kVrBuild, DocumentsDir(), gameDir)",
    'GetPrivateProfileStringA("General", "sLanguage"',
    "tolower",
    "LastNonEmptyIndex(values)",
    "no language found in any INI",
)
assert 'GetEnvironmentVariableA("USERPROFILE"' not in TRANSLATIONS
assert "#ifdef PRISMAUI_FO4VR\n        constexpr bool kVrBuild = true;" in TRANSLATIONS
documents = function_body(TRANSLATIONS, "std::string DocumentsDir()")
assert "SHGetKnownFolderPath(FOLDERID_Documents" in documents
assert "CoTaskMemFree(documents)" in documents
assert "Fallout4Prefs" not in TRANSLATIONS and "Fallout4Prefs" not in TRANSLATIONS_LANGUAGE
candidates = function_body(TRANSLATIONS_LANGUAGE, "std::vector<std::string> LanguageIniCandidates(")
assert_order(candidates, 'vr ? gameIni : profile + "Fallout4.ini"', 'profile + "Fallout4Custom.ini"')
assert r'(vr ? "\\My Games\\Fallout4VR\\" : "\\My Games\\Fallout4\\")' in candidates
assert r'vr ? gameIni : profile + "Fallout4.ini"' in candidates
last = function_body(TRANSLATIONS_LANGUAGE, "size_t LastNonEmptyIndex(")
assert "for (size_t index = values.size(); index-- > 0;)" in last and "!values[index].empty()" in last
assert '("translations_language_tests", "-Isrc", "tests/translations_language_tests.cpp", "src/PrismaUI/TranslationsLanguage.cpp"),' in CI_GATE

parse = function_body(TRANSLATIONS, "std::unordered_map<std::string, std::string> ParseTranslationFile(")
assert r'dataDir + "\\Interface\\Translations\\" + pluginName + "_" + lang + ".txt"' in parse
assert r'"_" + FALLBACK_LANG + ".txt"' in parse
assert "std::ifstream file(filePath, std::ios::binary)" in parse
assert "file.open(fallbackPath, std::ios::binary)" in parse
assert "0xFF" in parse and "raw[1]" in parse
assert "ParseBethesdaTranslation(raw, result)" in parse
assert "WideCharToMultiByte(CP_UTF8" not in parse
assert "ReadTranslationResource(resourcePath, json)" in TRANSLATIONS
assert "RE::BSResourceNiBinaryStream" in TRANSLATIONS
assert "BinaryStreamWithRescan" not in TRANSLATIONS

assert "Utf16LeToUtf8" in TRANSLATIONS_TEXT
assert "line[0] == '$'" in TRANSLATIONS_TEXT
assert r"line.find('\t')" in TRANSLATIONS_TEXT
assert "result[line.substr(0, tab)] = line.substr(tab + 1)" in TRANSLATIONS_TEXT
assert "raw.size() < 2" in TRANSLATIONS_TEXT

v3_escape = function_body(TRANSLATIONS_SCRIPT, "std::string EscapeJavaScriptV3(")
for needle in (r'result += "\\\\";', r'result += "\\\"";', r'result += "\\n";', r'result += "\\r";'):
    assert needle in v3_escape
assert "c < 0x20" not in v3_escape

script = function_body(TRANSLATIONS_SCRIPT, "std::string BuildL10NScript(")
assert "translations.empty()" in script
assert 'script += "window.L10N={"' in script
assert "EscapeJavaScriptV3(kv.first)" in script
assert "EscapeJavaScriptV3(kv.second)" in script
assert "window.t=function(k)" in script
assert "window.L10N[k]!==undefined?window.L10N[k]:k" in script

escape = function_body(TRANSLATIONS_SCRIPT, "std::string EscapeJavaScript(")
for needle in (r'result += "\\\\";', r'result += "\\\"";', r'result += "\\n";', r'result += "\\r";'):
    assert needle in escape
assert 'c < 0x20' in escape

assert "std::string BuildV4L10NScript" in TRANSLATIONS_SCRIPT
assert "translations_v3_script_test.cpp" in CI_GATE
assert "translations_v4_script_emitter.cpp" in CI_GATE
assert "src/PrismaUI/TranslationsScript.cpp" in CI_GATE

v4_script = TRANSLATIONS_SCRIPT[TRANSLATIONS_SCRIPT.index("std::string BuildV4L10NScript("):]
for needle in (
    r'window.PrismaL10N={locale:\"',
    r'messages:Object.create(null)',
    r'window.PrismaL10N.messages[\"',
    "window.PrismaL10N.t=function(k,vars)",
    "window.PrismaL10N.messages[k]",
    "Object.prototype.hasOwnProperty.call",
    "String(vars[name])",
    r'\"{{\"+name+\"}}\"',
):
    assert needle in v4_script

assert "class Parser" in TRANSLATIONS_JSON
assert "ParseObject({}, result, 0)" in TRANSLATIONS_JSON
assert "prefix + \".\" + key" in TRANSLATIONS_JSON
assert "!result.emplace(path, std::move(value)).second" in TRANSLATIONS_JSON
assert "input_[position_] == '{'" in TRANSLATIONS_JSON

assert "RegisterTranslationsV4" in PUBLIC_L10N
assert '"PrismaUI_F4_RegisterTranslationsV4"' in PUBLIC_L10N
assert "PrismaUI_F4_RegisterTranslationsV4" in MAIN
assert "PrismaUI_F4_RegisterTranslationsV4" in VR_MAIN
assert "bool PluginAPI::RegisterTranslationsV4" in API
v4 = function_body(API, "bool PluginAPI::RegisterTranslationsV4(")
assert_order(v4, "ParseJsonTranslationFile(plugin, lang)", "BuildV4L10NScript(entries, lang)", "RegisterLocalizationScript(view, script)")

assert "RegisterLocalizationScript(ViewId view, const std::string& script)" in WEB_RUNTIME_HEADER
assert "RegisterLocalizationScript(ViewId view, const std::string& script)" in WEB_RUNTIME
assert "virtual void RegisterLocalizationScript(ViewId, const std::string& script) = 0;" in BACKEND_API
assert "std::string localizationScript;" in VIEW_MANAGER
assert "void UltralightBackend::RegisterLocalizationScript(" in BACKEND
ready = function_body(DOCUMENT_CALLBACKS, "void UltralightDocumentCallbacks::OnWindowObjectReady(")
assert_order(ready, "trustedDocumentReady = true", "localizationScript", "InstallListeners")
loading = function_body(DOCUMENT_CALLBACKS, "void UltralightDocumentCallbacks::OnBeginLoading(")
assert_order(loading, "navigationBlocked = false", "trustedDocumentReady = false", "SetDocumentHealth")

invoke = function_body(WEB_RUNTIME, "void Invoke(ViewId view, const std::string& script,")
assert "st.backend->EvaluateScript(view, script" in invoke
assert "Host ABI is unavailable" in invoke
assert "Invoke target view is not live" in invoke

show_acceptance = function_body(ACCEPTANCE_MAIN, "void ShowView(PrismaView view)")
assert "g_registered" not in show_acceptance
assert "Focus(" not in show_acceptance
assert "void PublishRegistration()" in ACCEPTANCE_MAIN
assert "window.PrismaLocalizationAcceptance={registration:true};" in ACCEPTANCE_MAIN
assert "window.PrismaLocalizationAcceptance={registration:false};" in ACCEPTANCE_MAIN
assert "PrismaLocalizationAcceptanceVR" in ACCEPTANCE_XMAKE
assert 'add_deps("commonlibf4-ng")' in ACCEPTANCE_XMAKE
assert "PRISMA_LOCALIZATION_ACCEPTANCE_VR=1" in ACCEPTANCE_XMAKE
assert 'os.isfile(path.join(os.scriptdir(), "../../lib/commonlibf4vr/xmake.lua"))' in ACCEPTANCE_XMAKE
assert "registration" in ACCEPTANCE_PAGE
assert "FROM_FALLBACK_BA2_ENGLISH" in FALLBACK_BA2
assert "NESTED_FALLBACK_BA2_ENGLISH" in FALLBACK_BA2
assert 'includes("prismaui-core/tests/localization_acceptance")' in FRAMEWORK_XMAKE
assert "PRISMA_LOCALIZATION_ACCEPTANCE" in FRAMEWORK_XMAKE

print("V3 translation contract guard passed")
