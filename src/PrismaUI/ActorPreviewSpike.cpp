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
        Engine::OffscreenSlotPoll slot{};
    };

    Engine::OffscreenSlotPoll SehPoll(void* renderer, bool* faulted) {
        Engine::OffscreenSlotPoll poll{};
        __try {
            poll = Engine::PollOffscreenSlot(renderer);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            if (faulted) *faulted = true;
        }
        return poll;
    }

    void SehRun(Stages* out) {
        __try {
            void* renderer = Engine::GetOrCreateActorPreviewRenderer();
            out->renderer = renderer;
            if (!renderer) return;

            out->attached = Engine::AttachPlayer3DToRenderer(renderer);
            if (!out->attached) return;

            out->enabled = Engine::EnableActorPreview(renderer);
            out->slot = Engine::PollOffscreenSlot(renderer);
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
    bool  g_slotAssigned = false;
    std::uint32_t g_polls = 0;

    void PollRunningSlot() {
        if (!g_running || !g_renderer || g_slotAssigned) return;
        ++g_polls;

        bool faulted = false;
        const auto poll = SehPoll(g_renderer, &faulted);
        if (faulted) {
            g_slotAssigned = true;
            logger::warn("[ActorPreviewSpike] the renderer faulted (likely stale) on poll {} -- torn down "
                         "while running, so the slot was never read. Polling stopped.",
                         g_polls);
            return;
        }
        if (poll.state == Engine::Interface3DOffscreenSlotPolicy::SlotState::UnknownLayout) {
            g_slotAssigned = true;
            return;
        }
        if (poll.state != Engine::Interface3DOffscreenSlotPolicy::SlotState::Valid) return;
        g_slotAssigned = true;
        logger::info("[ActorPreviewSpike] offscreen slot assigned: raw={} on poll {}", poll.slot, g_polls);
    }

}

void Tick() {
    if (!Enabled()) return;
    if (!REX::FModule::IsRuntimeOG() && !REX::FModule::IsRuntimeAE() && !REX::FModule::IsRuntimeNG()) return;

    PollRunningSlot();

    static bool wasDown = false;
    const bool  down = (::GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0 && (::GetAsyncKeyState(HotkeyVK()) & 0x8000) != 0;
    const bool  edge = down && !wasDown;
    wasDown = down;
    if (!edge) return;

    if (g_running) {
        logger::info("[ActorPreviewSpike] === STOP === detach + disable");
        SehStop(g_renderer);
        g_running = false;
        g_slotAssigned = false;
        g_polls = 0;
        return;
    }

    logger::info("[ActorPreviewSpike] === RUN === Create -> AttachPlayer3D -> Enable");
    Stages s{};
    SehRun(&s);

    logger::info("[ActorPreviewSpike] stages: renderer={} attachedPlayer3D={} enabled={} slot={} raw={}",
                 s.renderer ? "ok" : "null", s.attached, s.enabled,
                 Engine::Interface3DOffscreenSlotPolicy::Name(s.slot.state), s.slot.slot);

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
    g_slotAssigned = false;
    g_polls = 0;
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
    g_slotAssigned = false;
    g_polls = 0;
}

}
