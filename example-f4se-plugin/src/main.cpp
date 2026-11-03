// example-f4se-plugin/src/main.cpp
// PrismaUI_F4 example plugin (CommonLibF4, xmake build).
// F4SE_PLUGIN_LOAD replaces F4SEPlugin_Query/Load pair.
// F4SE::Init() handles logging; REX:: macros for logging.
#include "PCH.h"
#include "PrismaUI_F4_API.h"
#include "PrismaUI_F4_Helper.h"
#include "keyhandler/keyhandler.h"
#include <windows.h>

static PRISMA_UI_API::IVPrismaUI10* g_api  = nullptr;
static PrismaView                   g_view = 0;
static bool                         g_visible = false;

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
    // BindUIEvent and RegisterJSListener callbacks both fire on the game thread; RE:: access
    // is safe directly inside them, no AddTask needed. window.prisma is injected by
    // PrismaUI_F4 before this callback.

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

    // View doesn't exist or is invalid - create new
    if (g_view == 0 || !g_api->IsValid(g_view)) {
        g_view = g_api->CreateView("PrismaUI-F4-Example/index.html", OnDomReady);
        if (g_view == 0) {
            REX::CRITICAL("CreateViews: failed to create view - PrismaUI_F4 API returned invalid handle");
            return;
        }
        // Declare this view as an interactive panel so IsAnyPanelVisible() counts it correctly.
        // Without a role, other plugins cannot detect that this view is on screen.
        g_api->SetViewRole(g_view, PRISMA_UI_API::ViewRole::kPanel);
        // Views start visible - hide immediately and show explicitly on toggle.
        g_api->Hide(g_view);
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
        g_api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI10>();
        if (!g_api) {
            REX::CRITICAL("PrismaUI_F4 V10 not found - update PrismaUI_F4.dll");
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
    F4SE::GetMessagingInterface()->RegisterListener(F4SEMessageHandler);
    return true;
}
