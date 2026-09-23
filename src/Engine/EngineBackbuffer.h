#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <RE/Fallout.h>

namespace PrismaUI::Engine {

    struct EngineBackbuffer {
        ID3D11RenderTargetView* rtv = nullptr;
        UINT width = 0;
        UINT height = 0;
        bool valid = false;
    };

    inline EngineBackbuffer ResolveEngineBackbuffer() {
        EngineBackbuffer out;
        auto* rd = RE::BSGraphics::GetRendererData();
        if (!rd) return out;
        auto* rtvRaw = reinterpret_cast<ID3D11RenderTargetView*>(
            rd->renderWindow[0].swapChainRenderTarget.rtView);
        if (!rtvRaw) return out;
        Microsoft::WRL::ComPtr<ID3D11Resource> res;
        rtvRaw->GetResource(&res);
        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        if (res && SUCCEEDED(res.As(&tex)) && tex) {
            D3D11_TEXTURE2D_DESC desc{};
            tex->GetDesc(&desc);
            if (desc.Width > 0 && desc.Height > 0) {
                out.rtv = rtvRaw;
                out.width = desc.Width;
                out.height = desc.Height;
                out.valid = true;
            }
        }
        return out;
    }

    inline ID3D11Device* GetLiveRenderDevice() {
        if (auto* rd = RE::BSGraphics::GetRendererData()) {
            return reinterpret_cast<ID3D11Device*>(rd->device);
        }
        return nullptr;
    }

}
