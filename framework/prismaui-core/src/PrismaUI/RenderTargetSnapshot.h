#pragma once

#include <d3d11.h>
#include <dxgiformat.h>
#include <wrl/client.h>

#include <cmath>
#include <cstdint>

namespace PrismaUI::Web {

    struct RenderTargetSnapshot {
        struct SampleUv {
            float u = 0.0f;
            float v = 0.0f;
        };

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;

        uint32_t viewportWidth = 0;
        uint32_t viewportHeight = 0;
        uint32_t textureWidth = 0;
        uint32_t textureHeight = 0;

        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

        float uvLeft = 0.0f;
        float uvTop = 0.0f;
        float uvRight = 1.0f;
        float uvBottom = 1.0f;

        uint64_t publishGeneration = 0;
        uint64_t contentGeneration = 0;
        uint64_t deviceEpoch = 0;

        [[nodiscard]] bool valid() const noexcept {
            return srv != nullptr && publishGeneration != 0 && viewportWidth > 0 && viewportHeight > 0 &&
                   textureWidth > 0 && textureHeight > 0 && format != DXGI_FORMAT_UNKNOWN &&
                   viewportWidth <= textureWidth && viewportHeight <= textureHeight && uvLeft >= 0.0f &&
                   uvTop >= 0.0f && uvRight <= 1.0f && uvBottom <= 1.0f && uvLeft < uvRight && uvTop < uvBottom;
        }

        [[nodiscard]] bool validFor(uint64_t generation) const noexcept {
            return generation != 0 && publishGeneration == generation && valid();
        }

        [[nodiscard]] bool validForEpoch(uint64_t generation, uint64_t epoch) const noexcept {
            return epoch != 0 && deviceEpoch == epoch && validFor(generation);
        }

        [[nodiscard]] bool hasViewportSizedUv() const noexcept {
            if (viewportWidth == 0 || viewportHeight == 0 || textureWidth == 0 || textureHeight == 0 || uvLeft < 0.0f ||
                uvTop < 0.0f || uvRight > 1.0f || uvBottom > 1.0f || uvLeft >= uvRight || uvTop >= uvBottom) {
                return false;
            }
            constexpr float tolerance = 0.01f;
            const float uvWidth = (uvRight - uvLeft) * static_cast<float>(textureWidth);
            const float uvHeight = (uvBottom - uvTop) * static_cast<float>(textureHeight);
            return std::fabs(uvWidth - static_cast<float>(viewportWidth)) <= tolerance &&
                   std::fabs(uvHeight - static_cast<float>(viewportHeight)) <= tolerance;
        }

        [[nodiscard]] bool validForComposition(uint64_t generation) const noexcept {
            return validFor(generation) && hasViewportSizedUv();
        }

        [[nodiscard]] SampleUv MapLogicalUv(float u, float v) const noexcept {
            return {uvLeft + (uvRight - uvLeft) * u, uvTop + (uvBottom - uvTop) * v};
        }
    };

}
