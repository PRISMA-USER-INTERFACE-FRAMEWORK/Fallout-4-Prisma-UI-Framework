// example-f4se-plugin/src/main.cpp
// PrismaUI_F4 integration using NewCommonLib.
// F4SE_PLUGIN_LOAD replaces F4SEPlugin_Query/Load pair.
// F4SE::Init() handles logging; REX:: macros for logging.
#include "PCH.h"
#include "PrismaUI_F4_API.h"
#include "../../src/PluginRuntimePolicy.h"
#include "PrismaUI_F4_Helper.h"
#include "keyhandler/keyhandler.h"
#include <windows.h>

static PRISMA_UI_API::IVPrismaUI4* g_api  = nullptr;
static PrismaView                   g_view = 0;
static bool                         g_visible = false;

static void ResolvePrismaGlobal(float value) {
    if (!g_api || !g_api->IsValid(g_view)) return;
    char script[256];
    snprintf(script, sizeof(script), "if(window._prismaResolveGlobal)window._prismaResolveGlobal(%.6f);", value);
    g_api->Invoke(g_view, script);
}

static void ResolvePrismaProperty(float value) {
    if (!g_api || !g_api->IsValid(g_view)) return;
    char script[256];
    snprintf(script, sizeof(script), "if(window._prismaResolveProperty)window._prismaResolveProperty(%.6f);", value);
    g_api->Invoke(g_view, script);
}

static void ResolveGlobalWrite(bool success) {
    if (!g_api || !g_api->IsValid(g_view)) return;
    const char* result = success ? "true" : "false";
    char script[128];
    snprintf(script, sizeof(script), "if(window._resolveGlobalWrite)window._resolveGlobalWrite(%s);", result);
    g_api->Invoke(g_view, script);
}

static bool CopyToSystemClipboard(const std::string& text) {
    if (!OpenClipboard(nullptr)) {
        REX::WARN("CopyToSystemClipboard: OpenClipboard failed");
        return false;
    }

    if (!EmptyClipboard()) {
        REX::WARN("CopyToSystemClipboard: EmptyClipboard failed");
        CloseClipboard();
        return false;
    }

    size_t len = text.length() + 1;
    HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, len);
    if (!hGlob) {
        REX::WARN("CopyToSystemClipboard: GlobalAlloc failed");
        CloseClipboard();
        return false;
    }

    char* pBuf = static_cast<char*>(GlobalLock(hGlob));
    if (!pBuf) {
        REX::WARN("CopyToSystemClipboard: GlobalLock failed");
        GlobalFree(hGlob);
        CloseClipboard();
        return false;
    }

    memcpy(pBuf, text.c_str(), len);
    GlobalUnlock(hGlob);

    if (!SetClipboardData(CF_TEXT, hGlob)) {
        REX::WARN("CopyToSystemClipboard: SetClipboardData failed");
        GlobalFree(hGlob);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    REX::INFO("CopyToSystemClipboard: success ({} bytes)", text.length());
    return true;
}

static void OnDomReady(PrismaView v)
{
    // BindUIEvent callbacks fire on game thread; RegisterJSListener on Ultralight thread.
    // window.prisma is injected by PrismaUI_F4 before this callback.

    g_api->RegisterConsoleCallback(v, [](PrismaView, PRISMA_UI_API::ConsoleMessageLevel lvl, const char* msg) {
        switch (lvl) {
        case PRISMA_UI_API::ConsoleMessageLevel::Error:   REX::CRITICAL("[JS] {}", msg); break;
        case PRISMA_UI_API::ConsoleMessageLevel::Warning: REX::WARN("[JS] {}",  msg); break;
        default:                                          REX::INFO("[JS] {}",  msg); break;
        }
    });

    g_api->BindUIEvent(v, "requestClose", [](const char*) {
        REX::INFO("EVENT: requestClose fired");
        g_visible = false;
        REX::INFO("  Set g_visible = false");
        g_api->Unfocus(g_view);
        REX::INFO("  Called Unfocus");
        g_api->Hide(g_view);
        REX::INFO("  Called Hide");
    });

    g_api->BindUIEvent(v, "sendPrismaRequest", [](const char* request) {
        REX::INFO("====================================");
        REX::INFO("EVENT: sendPrismaRequest fired");
        REX::INFO("====================================");

        if (!request) {
            REX::CRITICAL("sendPrismaRequest: request pointer is NULL");
            return;
        }

        if (request[0] == '\0') {
            REX::WARN("sendPrismaRequest: request is empty string");
            return;
        }

        REX::INFO("Raw request: {}", request);

        std::string_view req(request);
        std::string type = PRISMA_UI_HELPER::GetJsonString(req, "type");

        REX::INFO("Parsed type: '{}'", type);

        if (type.empty()) {
            REX::WARN("sendPrismaRequest: type field is EMPTY");
            return;
        }

        if (type == "getGlobal") {
            std::string esp = PRISMA_UI_HELPER::GetJsonString(req, "esp");
            std::string fid = PRISMA_UI_HELPER::GetJsonString(req, "formId");

            REX::INFO("===== getGlobal REQUEST =====");
            REX::INFO("  esp: '{}'", esp);
            REX::INFO("  formId: '{}'", fid);

            if (esp.empty() || fid.empty()) {
                REX::WARN("getGlobal: MISSING REQUIRED FIELDS");
                ResolvePrismaGlobal(0.0f);
                return;
            }

            float value = 0.0f;
            try {
                auto formId = std::stoul(fid, nullptr, 16);
                REX::INFO("  Parsed formId as hex: 0x{:X}", formId);

                auto* form = RE::TESForm::GetFormByID(formId);
                if (!form) {
                    REX::WARN("getGlobal: FORM NOT FOUND (0x{:X})", formId);
                    ResolvePrismaGlobal(0.0f);
                    return;
                }
                REX::INFO("  Form found");

                auto* global = form->As<RE::TESGlobal>();
                if (!global) {
                    REX::WARN("getGlobal: FORM IS NOT A GLOBAL (0x{:X})", formId);
                    ResolvePrismaGlobal(0.0f);
                    return;
                }

                value = global->value;
                REX::INFO("getGlobal SUCCESS: {}:{} = {}", esp, fid, value);
            } catch (const std::exception& e) {
                REX::WARN("getGlobal EXCEPTION: {}", e.what());
            }

            ResolvePrismaGlobal(value);

        } else if (type == "setGlobal") {
            std::string esp = PRISMA_UI_HELPER::GetJsonString(req, "esp");
            std::string fid = PRISMA_UI_HELPER::GetJsonString(req, "formId");
            float value = PRISMA_UI_HELPER::GetJsonFloat(req, "value");

            REX::INFO("===== setGlobal REQUEST =====");
            REX::INFO("  esp: '{}'", esp);
            REX::INFO("  formId: '{}'", fid);
            REX::INFO("  value: {}", value);

            if (esp.empty() || fid.empty()) {
                REX::WARN("setGlobal: MISSING REQUIRED FIELDS");
                ResolveGlobalWrite(false);
                return;
            }

            try {
                auto formId = std::stoul(fid, nullptr, 16);
                REX::INFO("  Parsed formId as hex: 0x{:X}", formId);

                auto* form = RE::TESForm::GetFormByID(formId);
                if (!form) {
                    REX::WARN("setGlobal: FORM NOT FOUND (0x{:X})", formId);
                    ResolveGlobalWrite(false);
                    return;
                }
                REX::INFO("  Form found");

                auto* global = form->As<RE::TESGlobal>();
                if (!global) {
                    REX::WARN("setGlobal: FORM IS NOT A GLOBAL (0x{:X})", formId);
                    ResolveGlobalWrite(false);
                    return;
                }

                global->value = value;
                REX::INFO("setGlobal SUCCESS: {}:{} = {}", esp, fid, value);
                ResolveGlobalWrite(true);
            } catch (const std::exception& e) {
                REX::WARN("setGlobal EXCEPTION: {}", e.what());
                ResolveGlobalWrite(false);
            }

        } else if (type == "getProperty") {
            std::string esp = PRISMA_UI_HELPER::GetJsonString(req, "esp");
            std::string fid = PRISMA_UI_HELPER::GetJsonString(req, "formId");
            std::string script = PRISMA_UI_HELPER::GetJsonString(req, "script");
            std::string prop = PRISMA_UI_HELPER::GetJsonString(req, "property");

            REX::INFO("===== getProperty REQUEST =====");
            REX::INFO("  esp: '{}'", esp);
            REX::INFO("  formId: '{}'", fid);
            REX::INFO("  script: '{}'", script);
            REX::INFO("  property: '{}'", prop);

            if (esp.empty() || fid.empty() || script.empty() || prop.empty()) {
                REX::WARN("getProperty: MISSING REQUIRED FIELDS");
                ResolvePrismaProperty(0.0f);
                return;
            }

            float value = 0.0f;
            try {
                auto localFormId = std::stoul(fid, nullptr, 16);
                auto* handler = RE::TESDataHandler::GetSingleton();
                auto formId = handler->LookupFormID(localFormId, esp.c_str());
                REX::INFO("  Parsed formId as hex: 0x{:X} (full: 0x{:X})", localFormId, formId);

                auto* form = RE::TESForm::GetFormByID(formId);
                if (!form) {
                    REX::WARN("getProperty: FORM NOT FOUND (0x{:X})", formId);
                    ResolvePrismaProperty(0.0f);
                    return;
                }
                REX::INFO("  Form found (0x{:X})", formId);

                // Access BSScript VM to read property from attached script.
                auto* vm = RE::GameVM::GetSingleton()->GetVM().get();
                if (!vm) {
                    REX::WARN("getProperty: PAPYRUS VM NOT READY");
                    ResolvePrismaProperty(0.0f);
                    return;
                }
                REX::INFO("  Papyrus VM ready");

                auto handle = vm->GetObjectHandlePolicy().GetHandleForObject(static_cast<uint32_t>(form->GetFormType()), form);
                REX::INFO("  Object handle: {}", handle);

                auto* concreteVM = static_cast<RE::BSScript::Internal::VirtualMachine*>(vm);

                RE::BSAutoLock lock(concreteVM->attachedScriptsLock);
                auto it = concreteVM->attachedScripts.find(handle);

                if (it == concreteVM->attachedScripts.end()) {
                    REX::WARN("getProperty: NO SCRIPTS ATTACHED TO FORM");
                    ResolvePrismaProperty(0.0f);
                    return;
                }

                REX::INFO("  Found {} attached script(s)", it->second.size());

                for (auto& attached : it->second) {
                    auto* obj = attached.get();
                    if (!obj) continue;

                    auto* typeInfo = obj->GetTypeInfo();
                    if (!typeInfo) continue;

                    if (std::string_view(typeInfo->GetName()) != script) continue;

                    auto* prop_val = obj->GetProperty(RE::BSFixedString(prop.c_str()));
                    if (!prop_val) continue;

                    try {
                        value = RE::BSScript::get<float>(*prop_val);
                        REX::INFO("getProperty SUCCESS: {}:{}.{}.{} = {} (float)", esp, fid, script, prop, value);
                    } catch (...) {
                        try {
                            value = static_cast<float>(RE::BSScript::get<std::int32_t>(*prop_val));
                            REX::INFO("getProperty SUCCESS: {}:{}.{}.{} = {} (int)", esp, fid, script, prop, value);
                        } catch (...) {
                            try {
                                value = RE::BSScript::get<bool>(*prop_val) ? 1.0f : 0.0f;
                                REX::INFO("getProperty SUCCESS: {}:{}.{}.{} = {} (bool)", esp, fid, script, prop, value);
                            } catch (...) {
                                REX::WARN("getProperty: unsupported property type");
                                value = 0.0f;
                            }
                        }
                    }
                    ResolvePrismaProperty(value);
                    return;
                }

                REX::WARN("getProperty: No matching script found (looked for '{}' on form)", script);
                ResolvePrismaProperty(0.0f);

            } catch (const std::exception& e) {
                REX::WARN("getProperty EXCEPTION: {}", e.what());
                ResolvePrismaProperty(0.0f);
            }

        } else {
            REX::WARN("sendPrismaRequest: unknown type '{}'", type);
        }
    });

    g_api->BindUIEvent(v, "sendDataToF4SE", [](const char* data) {
        REX::INFO("EVENT: sendDataToF4SE fired");
        if (!data) {
            REX::WARN("  data pointer is NULL");
            return;
        }
        REX::INFO("  Raw data: {}", data);

        // Check if this is a diagnose command
        std::string cmd = PRISMA_UI_HELPER::GetJsonString(data, "cmd");
        if (cmd == "diagnose") {
            std::string esp = PRISMA_UI_HELPER::GetJsonString(data, "esp");
            std::string fid_str = PRISMA_UI_HELPER::GetJsonString(data, "formId");

            try {
                auto localFormId = std::stoul(fid_str, nullptr, 16);
                auto* handler = RE::TESDataHandler::GetSingleton();
                auto fullFormId = handler->LookupFormID(localFormId, esp.c_str());

                REX::CRITICAL("=== DIAGNOSIS ===");
                REX::CRITICAL("  Plugin: {}", esp);
                REX::CRITICAL("  Local FormID: 0x{:X}", localFormId);
                REX::CRITICAL("  Full FormID: 0x{:X}", fullFormId);

                auto* form = RE::TESForm::GetFormByID(fullFormId);
                if (!form) {
                    REX::CRITICAL("  ERROR: Form not found");
                    char script[256];
                    snprintf(script, sizeof(script), "if(window._showDiag)window._showDiag('Form not found - plugin loaded?');");
                    g_api->Invoke(g_view, script);
                    return;
                }

                REX::CRITICAL("  Form type: 0x{:X}", static_cast<uint32_t>(form->GetFormType()));

                auto* global = form->As<RE::TESGlobal>();
                if (global) {
                    REX::CRITICAL("  SUCCESS: TESGlobal = {}", global->value);
                    char script[256];
                    snprintf(script, sizeof(script), "if(window._showDiag)window._showDiag('SUCCESS: TESGlobal = %.2f');", global->value);
                    g_api->Invoke(g_view, script);
                } else {
                    REX::CRITICAL("  ERROR: Not a TESGlobal (type 0x{:X})", static_cast<uint32_t>(form->GetFormType()));
                    char script[256];
                    snprintf(script, sizeof(script), "if(window._showDiag)window._showDiag('NOT a TESGlobal - use getProperty()');");
                    g_api->Invoke(g_view, script);
                }
            } catch (const std::exception& e) {
                REX::CRITICAL("  EXCEPTION: {}", e.what());
                char script[256];
                snprintf(script, sizeof(script), "if(window._showDiag)window._showDiag('ERROR: Invalid input');");
                g_api->Invoke(g_view, script);
            }
            return;
        }

        if (cmd == "copyLog") {
            std::string_view dataStr(data);
            auto delimiterPos = dataStr.find('|');
            std::string logText = (delimiterPos != std::string_view::npos) ? std::string(dataStr.substr(delimiterPos + 1)) : "";
            REX::INFO("EVENT: copyLog fired ({} bytes)", logText.length());

            bool success = CopyToSystemClipboard(logText);

            char script[256];
            const char* result = success ? "true" : "false";
            snprintf(script, sizeof(script), "if(window._resolveCopyLog)window._resolveCopyLog(%s);", result);
            g_api->Invoke(g_view, script);
            return;
        }

        std::string msg = PRISMA_UI_HELPER::GetJsonString(data, "message");
        REX::INFO("  Parsed message: '{}'", msg);
        REX::INFO("Received from JS: {}", msg);

        char script[512];
        snprintf(script, sizeof(script), "if(window.lg)window.lg('pc','\\u2190C++','Message received: %s');", msg.c_str());
        g_api->Invoke(g_view, script);
    });


    g_api->Invoke(v, "init()");
    REX::INFO("View DOM ready: {}", v);
}

static void CreateViews()
{
    if (!g_api) return;

    // View doesn't exist or is invalid — create new
    if (g_view == 0 || !g_api->IsValid(g_view)) {
        g_view = g_api->CreateView("PrismaUI-F4-Example/index.html", OnDomReady);
        if (g_view == 0) {
            REX::CRITICAL("CreateViews: failed to create view — PrismaUI_F4 API returned invalid handle");
            return;
        }
        // Views start hidden — show explicitly on toggle.
    } else {
        REX::INFO("View reused on reload: {}", g_view);
    }
}

static void Toggle()
{
    if (!g_api || !g_api->IsValid(g_view)) {
        REX::WARN("Toggle: view not ready (g_api={}, g_view={})", g_api ? "ready" : "null", g_view);
        return;
    }
    g_visible = !g_visible;
    if (g_visible) {
        g_api->Show(g_view);
        g_api->Focus(g_view, /*pauseGame=*/false);
        g_api->Invoke(g_view, "updateFocusLabel('Focused. Press F3 to close.')");
    } else {
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
    }
}

static void F4SEMessageHandler(F4SE::MessagingInterface::Message* message)
{
    switch (message->type) {

    case F4SE::MessagingInterface::kGameDataReady:
    {
        g_api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI4>();
        if (!g_api) {
            REX::CRITICAL("PrismaUI_F4 API not found — is PrismaUI_F4.dll installed?");
            return;
        }

        KeyHandler::RegisterSink();
        [[maybe_unused]] auto _ = KeyHandler::GetSingleton()->Register(
            static_cast<uint32_t>(RE::BS_BUTTON_CODE::kF3), KeyEventType::KEY_DOWN, Toggle);
        break;
    }

    case F4SE::MessagingInterface::kPostLoadGame:
    case F4SE::MessagingInterface::kNewGame:
        if (g_api) CreateViews();
        break;

    default:
        break;
    }
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_intfc)
{
    F4SE::Init(a_intfc);
    const auto runtime = a_intfc->RuntimeVersion();
    if (!PrismaUI::PluginRuntimePolicy::IsOgOrAeBuild(runtime.major(), runtime.minor(), runtime.patch())) {
        REX::CRITICAL("PrismaUI example refuses game version {}.{}.{}: only OG 1.10.163 and AE 1.11.137 or later are supported", runtime.major(), runtime.minor(), runtime.patch());
        return false;
    }
    F4SE::GetMessagingInterface()->RegisterListener(F4SEMessageHandler);
    return true;
}
