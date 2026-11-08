#pragma once

#include <cstdint>
#include <d3d11.h>

namespace PrismaUI::Engine {

    inline ID3D11ShaderResourceView* ResolveEngineTextureSRV(void* bsGraphicsTexture) {
        if (!bsGraphicsTexture) return nullptr;
        __try {

            auto* srv = *reinterpret_cast<ID3D11ShaderResourceView**>(
                reinterpret_cast<std::uint8_t*>(bsGraphicsTexture) + 0x00);
            if (!srv) return nullptr;
            ID3D11ShaderResourceView* qi = nullptr;
            if (SUCCEEDED(srv->QueryInterface(__uuidof(ID3D11ShaderResourceView),
                                              reinterpret_cast<void**>(&qi))) && qi) {
                qi->Release();
                if (qi == srv) return srv;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        return nullptr;
    }

    inline ID3D11ShaderResourceView* PeekEngineTextureSRV(void* bsGraphicsTexture) {
        if (!bsGraphicsTexture) return nullptr;
        return *reinterpret_cast<ID3D11ShaderResourceView**>(
            reinterpret_cast<std::uint8_t*>(bsGraphicsTexture) + 0x00);
    }

    inline void PokeEngineTextureSRV(void* bsGraphicsTexture, ID3D11ShaderResourceView* srv) {
        if (!bsGraphicsTexture) return;
        *reinterpret_cast<ID3D11ShaderResourceView**>(
            reinterpret_cast<std::uint8_t*>(bsGraphicsTexture) + 0x00) = srv;
    }

}
