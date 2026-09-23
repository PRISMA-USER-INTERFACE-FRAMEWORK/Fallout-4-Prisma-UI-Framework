#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <d3d11.h>

#include <RE/Fallout.h>

#include "RE/N/NiTexture.h"

namespace PrismaUI::Engine {

    struct PrivateTextureClone {
        RE::NiTexture* clone = nullptr;
        void* rendererTexture = nullptr;

        explicit operator bool() const { return clone != nullptr; }
    };

    inline void SetPrivateTextureSRV(void* rendererTexture, ID3D11ShaderResourceView* srv) {
        if (!rendererTexture) return;
        *reinterpret_cast<ID3D11ShaderResourceView**>(
            reinterpret_cast<std::uint8_t*>(rendererTexture) + 0x00) = srv;
    }

    inline PrivateTextureClone MakePrivateTextureClone(RE::NiTexture* donor,
                                                       ID3D11ShaderResourceView* srv) {
        if (!donor) return {};

        auto* renderer = std::calloc(1, 0x80);
        if (!renderer) return {};
        SetPrivateTextureSRV(renderer, srv);

        auto* clone = static_cast<RE::NiTexture*>(std::calloc(1, sizeof(RE::NiTexture)));
        if (!clone) { std::free(renderer); return {}; }
        std::memcpy(clone, donor, sizeof(RE::NiTexture));

        auto* raw = reinterpret_cast<std::uint8_t*>(clone);
        *reinterpret_cast<std::uint32_t*>(raw + 0x08) = 0x40000000;
        *reinterpret_cast<void**>(raw + 0x10) = nullptr;
        *reinterpret_cast<void**>(raw + 0x20) = nullptr;
        *reinterpret_cast<void**>(raw + 0x28) = nullptr;
        *reinterpret_cast<void**>(raw + 0x30) = nullptr;
        clone->rendererTexture = static_cast<RE::BSGraphics::Texture*>(renderer);

        return { clone, renderer };
    }

}
