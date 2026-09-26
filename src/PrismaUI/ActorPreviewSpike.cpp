#include "ActorPreviewSpike.h"

#include <windows.h>

#include <string>

#include "../Engine/EngineInterface3D.h"
#include "Utils/ModulePath.h"

namespace PrismaUI::ActorPreviewSpike {
namespace {

    struct Stages {
        void* renderer = nullptr;
        bool  attached = false;
        bool  enabled = false;
    };

    void SehRun(Stages* out) {
        __try {
            void* renderer = Engine::GetOrCreateActorPreviewRenderer();
            out->renderer = renderer;
            if (!renderer) return;

            out->attached = Engine::AttachPlayer3DToRenderer(renderer);
            if (!out->attached) return;

            out->enabled = Engine::EnableActorPreview(renderer);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    void SehStop(void* renderer) {
        if (!renderer) return;
        __try {
            Engine::StopActorPreview(renderer);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    std::wstring IniPath() {
        return (Utils::ThisModuleDir() / L"PrismaUI_ActorPreviewSpike.ini").wstring();
    }

    bool Enabled() {
        static int cached = -1;
        if (cached < 0) {
            cached = ::GetPrivateProfileIntW(L"Spike", L"enabled", 0, IniPath().c_str()) ? 1 : 0;
            logger::info("[ActorPreviewSpike] enabled={}", cached);
        }
        return cached == 1;
    }

    int HotkeyVK() {
        static int vk = -1;

        if (vk < 0) vk = ::GetPrivateProfileIntW(L"Spike", L"hotkey", VK_NUMPAD3, IniPath().c_str());
        return vk;
    }

    void* g_renderer = nullptr;
    bool  g_running = false;

}

void Tick() {
    if (!Enabled()) return;
    if (!REX::FModule::IsRuntimeOG() && !REX::FModule::IsRuntimeAE()) return;

    static bool wasDown = false;
    const bool  down = (::GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0 && (::GetAsyncKeyState(HotkeyVK()) & 0x8000) != 0;
    const bool  edge = down && !wasDown;
    wasDown = down;
    if (!edge) return;

    if (g_running) {
        logger::info("[ActorPreviewSpike] === STOP === detach + disable");
        SehStop(g_renderer);
        g_running = false;
        return;
    }

    logger::info("[ActorPreviewSpike] === RUN === Create -> AttachPlayer3D -> Enable");
    Stages s{};
    SehRun(&s);

    logger::info("[ActorPreviewSpike] stages: renderer={} attachedPlayer3D={} enabled={}",
                 s.renderer ? "ok" : "null", s.attached, s.enabled);

    if (!s.renderer) {
        logger::warn("[ActorPreviewSpike] no renderer -- Create/GetByName returned null. Either the "
                     "runtime has no Interface3D system, or the engine refused another instance.");
        return;
    }
    if (!s.attached) {
        logger::warn("[ActorPreviewSpike] renderer exists but the player has no 3D yet. Retry in a "
                     "loaded save with the player rendered, not on a loading screen or main menu.");
        return;
    }

    g_renderer = s.renderer;
    g_running = s.enabled;
    logger::info("[ActorPreviewSpike] armed. What to look for: the player's model drawn through the "
                 "UI renderer. Nothing visible is a RESULT, not a non-answer -- it means the attach "
                 "and enable both succeeded and the draw did not, which is the next thing to chase "
                 "(camera framing and lighting are unset in this increment).");
}

void Shutdown() {
    if (!g_renderer) return;
    logger::info("[ActorPreviewSpike] shutdown -- releasing the player reference");
    SehStop(g_renderer);
    g_renderer = nullptr;
    g_running = false;
}

}
