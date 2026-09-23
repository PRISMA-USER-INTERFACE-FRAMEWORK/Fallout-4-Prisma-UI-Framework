#include "LocalMapSpike.h"

#ifdef PRISMA_DIAGNOSTICS

#include <windows.h>
#include <d3d11.h>
#include <DirectXTK/ScreenGrab.h>

#include <string>

#include "../Engine/EngineLocalMap.h"
#include "../Engine/EngineTexture.h"
#include "Utils/ModulePath.h"

namespace PrismaUI::LocalMapSpike {
namespace {

    void SehRender(void** outNi, void** outSrv, ID3D11Resource** outRes, void** outRenderer) {
        *outNi = nullptr; *outSrv = nullptr; *outRes = nullptr; *outRenderer = nullptr;
        __try {
            auto renderer = Engine::LocalMapRenderer::Create();
            *outRenderer = renderer.raw();
            if (!renderer.valid()) return;
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
        return (Utils::ThisModuleDir() / L"PrismaUI_LocalMapSpike.ini").wstring();
    }

    bool Enabled() {
        static int cached = -1;
        if (cached < 0) {
            const std::wstring ini = IniPath();
            cached = ::GetPrivateProfileIntW(L"Spike", L"enabled", 0, ini.c_str()) ? 1 : 0;
            const int len = ::WideCharToMultiByte(CP_UTF8, 0, ini.c_str(), static_cast<int>(ini.size()),
                                                  nullptr, 0, nullptr, nullptr);
            std::string iniUtf8(len > 0 ? len : 0, '\0');
            if (len > 0)
                ::WideCharToMultiByte(CP_UTF8, 0, ini.c_str(), static_cast<int>(ini.size()),
                                      iniUtf8.data(), len, nullptr, nullptr);
            logger::info("[LocalMapSpike] ini='{}' enabled={}", iniUtf8, cached);
        }
        return cached == 1;
    }
    int HotkeyVK() {
        static int vk = -1;
        if (vk < 0)
            vk = ::GetPrivateProfileIntW(L"Spike", L"hotkey", VK_NUMPAD1, IniPath().c_str());
        return vk;
    }
}

void Tick(ID3D11Device* , ID3D11DeviceContext* ctx) {
    if (!Enabled() || !ctx || (!REX::FModule::IsRuntimeOG() && !REX::FModule::IsRuntimeAE())) return;

    static bool wasDown = false;
    const bool down = (::GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0 && (::GetAsyncKeyState(HotkeyVK()) & 0x8000) != 0;
    const bool edge = down && !wasDown;
    wasDown = down;
    if (!edge) return;

    logger::info("[LocalMapSpike] === RUN (hotkey Shift+chord) === InitRenderer -> Render -> dump");
    void* ni = nullptr; void* srv = nullptr; ID3D11Resource* res = nullptr; void* renderer = nullptr;
    SehRender(&ni, &srv, &res, &renderer);

    logger::info("[LocalMapSpike] stages: InitRenderer={} Render->NiTexture={} SRV={} Resource={}",
                 renderer != nullptr, ni != nullptr, srv != nullptr, res != nullptr);

    if (res) {

        const HRESULT hr = DirectX::SaveDDSTextureToFile(ctx, res, L"PrismaLocalMapSpike.dds");
        D3D11_TEXTURE2D_DESC d{};
        if (ID3D11Texture2D* tex2d = nullptr; SUCCEEDED(res->QueryInterface(__uuidof(ID3D11Texture2D),
                                                        reinterpret_cast<void**>(&tex2d))) && tex2d) {
            tex2d->GetDesc(&d); tex2d->Release();
        }
        logger::info("[LocalMapSpike] Render produced a texture ({}x{}, fmt={}); SaveDDS hr={:#x} -> "
                     "PrismaLocalMapSpike.dds (game root)", d.Width, d.Height, (int)d.Format,
                     static_cast<std::uint32_t>(hr));
        res->Release();
    } else {
        logger::warn("[LocalMapSpike] Render produced NO texture (renderer={}) -- either init failed or "
                     "Render needs context this path doesn't have", renderer != nullptr);
    }

    SehDestroy(renderer);
    logger::info("[LocalMapSpike] === DONE (no crash reached this line) ===");
}
}

#endif