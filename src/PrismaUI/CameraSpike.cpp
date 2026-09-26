#include "CameraSpike.h"

#ifdef PRISMA_DIAGNOSTICS

#include <windows.h>
#include <d3d11.h>
#include <DirectXTK/ScreenGrab.h>

#include <string>

#include "RE/N/NiPoint3.h"
#include "RE/N/NiTexture.h"
#include "RE/P/PlayerCharacter.h"

#include "../Engine/EngineLocalMap.h"
#include "../Engine/EngineTexture.h"
#include "Utils/ModulePath.h"

namespace PrismaUI::CameraSpike {
namespace {

    constexpr float kZExtent  = 4096.0f;
    constexpr float kXYExtent = 4096.0f;
    constexpr float kAspect   = 9.0f / 16.0f;

    struct Cfg { float halfW; };
    constexpr Cfg kSweep[] = {
        { 128.0f }, { 192.0f }, { 256.0f }, { 384.0f }, { 512.0f }, { 768.0f },
    };

    bool PlayerPos(RE::NiPoint3& out) {
        __try {
            auto* pc = RE::PlayerCharacter::GetSingleton();
            if (!pc) return false;
            out = pc->GetPosition();
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    void SehRenderCfg(const RE::NiPoint3& center, const Cfg& cfg,
                      void** outNi, void** outSrv, ID3D11Resource** outRes, void** outRenderer) {
        *outNi = nullptr; *outSrv = nullptr; *outRes = nullptr; *outRenderer = nullptr;
        __try {
            auto renderer = Engine::LocalMapRenderer::Create();
            *outRenderer = renderer.raw();
            if (!renderer.valid()) return;

            RE::NiPoint3 c = center;
            renderer.SetInitialPosition(c);

            RE::NiPoint3 cmax{ c.x + kXYExtent, c.y + kXYExtent, c.z + kZExtent };
            RE::NiPoint3 cmin{ c.x - kXYExtent, c.y - kXYExtent, c.z - kZExtent };
            renderer.SetExtents(cmax, cmin);

            renderer.SetMinFrustum(cfg.halfW * 2.0f, cfg.halfW * kAspect * 2.0f);
            renderer.SetZoom(1.0f);

            RE::NiTexture* ni = renderer.Render();
            *outNi = ni;
            if (!ni) return;
            auto* srv = Engine::ResolveEngineTextureSRV(ni->rendererTexture);
            *outSrv = srv;
            if (!srv) return;
            ID3D11Resource* res = nullptr;
            srv->GetResource(&res);
            *outRes = res;
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    void SehDestroy(void* renderer) {
        if (!renderer) return;
        __try {
            Engine::LocalMapRenderer::DestroyRaw(renderer);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    std::wstring IniPath() {
        return (Utils::ThisModuleDir() / L"PrismaUI_CameraSpike.ini").wstring();
    }

    bool Enabled() {
        static int cached = -1;
        if (cached < 0) {
            const std::wstring ini = IniPath();
            cached = ::GetPrivateProfileIntW(L"Spike", L"enabled", 0, ini.c_str()) ? 1 : 0;
        }
        return cached == 1;
    }
    int HotkeyVK() {
        static int vk = -1;
        if (vk < 0) vk = ::GetPrivateProfileIntW(L"Spike", L"hotkey", VK_NUMPAD2, IniPath().c_str());
        return vk;
    }
    float OffsetX()  { return static_cast<float>(::GetPrivateProfileIntW(L"Spike", L"offsetX", 0,    IniPath().c_str())); }
    float OffsetY()  { return static_cast<float>(::GetPrivateProfileIntW(L"Spike", L"offsetY", 1024, IniPath().c_str())); }
}

void Tick(ID3D11Device*, ID3D11DeviceContext* ctx) {
    if (!ctx || (!REX::FModule::IsRuntimeOG() && !REX::FModule::IsRuntimeAE()) || !Enabled()) return;

    static bool wasDown = false;
    const bool down = (::GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0 && (::GetAsyncKeyState(HotkeyVK()) & 0x8000) != 0;
    const bool edge = down && !wasDown;
    wasDown = down;
    if (!edge) return;

    RE::NiPoint3 player{};
    if (!PlayerPos(player)) { logger::warn("[CameraSpike] no player pos -- aborting run"); return; }
    RE::NiPoint3 target{ player.x + OffsetX(), player.y + OffsetY(), player.z };

    logger::info("[CameraSpike] === SWEEP === player=({:.0f},{:.0f},{:.0f}) target=({:.0f},{:.0f},{:.0f}) "
                 "-- {} configs -> PrismaCameraSpike_N.dds",
                 player.x, player.y, player.z, target.x, target.y, target.z,
                 static_cast<int>(std::size(kSweep)));

    for (std::size_t i = 0; i < std::size(kSweep); ++i) {
        const Cfg& cfg = kSweep[i];
        void* ni = nullptr; void* srv = nullptr; ID3D11Resource* res = nullptr; void* renderer = nullptr;
        SehRenderCfg(target, cfg, &ni, &srv, &res, &renderer);

        std::uint32_t w = 0, h = 0, hr = 0;
        if (res) {
            wchar_t path[64]{};
            swprintf_s(path, L"PrismaCameraSpike_%zu.dds", i);
            hr = static_cast<std::uint32_t>(DirectX::SaveDDSTextureToFile(ctx, res, path));
            if (ID3D11Texture2D* t = nullptr; SUCCEEDED(res->QueryInterface(__uuidof(ID3D11Texture2D),
                                                     reinterpret_cast<void**>(&t))) && t) {
                D3D11_TEXTURE2D_DESC d{}; t->GetDesc(&d); w = d.Width; h = d.Height; t->Release();
            }
            res->Release();
        }
        logger::info("[CameraSpike]  cfg{}: halfW={:.0f} (frame {:.0f}u across, zoom pinned 1.0) -> tex={} "
                     "({}x{}) SaveDDS={:#x} file=PrismaCameraSpike_{}.dds",
                     i, cfg.halfW, cfg.halfW * 2.0f, ni != nullptr, w, h, hr, i);
        SehDestroy(renderer);
    }

    logger::info("[CameraSpike] === SWEEP DONE (one-shot; all renderers destroyed, no lingering cost) ===");
}
}

#endif