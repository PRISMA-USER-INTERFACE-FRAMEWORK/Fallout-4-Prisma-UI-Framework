#include "PCH.h"

#include "VR/VRCompositorResources.h"

#include <cmath>
#include <memory>

namespace PrismaUI::VRCompositor
{
    using namespace Resources;

    namespace Resources
    {
        [[nodiscard]] Runtime& Get() noexcept
        {

            static auto* runtime = new Runtime();
            return *runtime;
        }

        [[nodiscard]] float Width(const RECT& rectangle) noexcept
        {
            return static_cast<float>(rectangle.right - rectangle.left);
        }

        [[nodiscard]] float Height(const RECT& rectangle) noexcept
        {
            return static_cast<float>(rectangle.bottom - rectangle.top);
        }

        [[nodiscard]] RECT EyeRect(
            const EyeBounds& bounds,
            UINT atlasWidth,
            UINT atlasHeight) noexcept
        {
            const auto scaleX = static_cast<float>(atlasWidth);
            const auto scaleY = static_cast<float>(atlasHeight);
            return {
                static_cast<LONG>(std::lround(bounds.uMin * scaleX)),
                static_cast<LONG>(std::lround(bounds.vMin * scaleY)),
                static_cast<LONG>(std::lround(bounds.uMax * scaleX)),
                static_cast<LONG>(std::lround(bounds.vMax * scaleY))
            };
        }

        [[nodiscard]] bool FiniteBounds(const EyeBounds& bounds) noexcept
        {
            return bounds.valid &&
                   std::isfinite(bounds.uMin) &&
                   std::isfinite(bounds.vMin) &&
                   std::isfinite(bounds.uMax) &&
                   std::isfinite(bounds.vMax) &&
                   bounds.uMax > bounds.uMin &&
                   bounds.vMax > bounds.vMin &&
                   bounds.uMin >= 0.0f &&
                   bounds.vMin >= 0.0f &&
                   bounds.uMax <= 1.0f &&
                   bounds.vMax <= 1.0f;
        }

        [[nodiscard]] bool EnsureRenderer() noexcept
        {
            auto& runtime = Get();
            if (!runtime.device || !runtime.context) {
                return false;
            }
            if (runtime.effect && runtime.batch && runtime.inputLayout &&
                runtime.commonStates) {
                return true;
            }

            try {
                runtime.commonStates =
                    std::make_unique<DirectX::CommonStates>(runtime.device);
                runtime.effect =
                    std::make_unique<DirectX::BasicEffect>(runtime.device);
                runtime.effect->SetTextureEnabled(true);
                runtime.effect->SetVertexColorEnabled(true);
                runtime.effect->SetLightingEnabled(false);

                const void* bytecode = nullptr;
                std::size_t bytecodeLength = 0;
                runtime.effect->GetVertexShaderBytecode(&bytecode, &bytecodeLength);
                if (!bytecode || bytecodeLength == 0) {
                    ReleaseDeviceResources();
                    return false;
                }
                if (FAILED(runtime.device->CreateInputLayout(
                        DirectX::VertexPositionColorTexture::InputElements,
                        DirectX::VertexPositionColorTexture::InputElementCount,
                        bytecode,
                        bytecodeLength,
                        runtime.inputLayout.GetAddressOf()))) {
                    ReleaseDeviceResources();
                    return false;
                }
                runtime.batch = std::make_unique<
                    DirectX::PrimitiveBatch<DirectX::VertexPositionColorTexture>>(
                    runtime.context);
                return true;
            } catch (...) {
                ReleaseDeviceResources();
                return false;
            }
        }
    }

    void ReleaseDeviceResources() noexcept
    {
        auto& runtime = Get();
        runtime.batch.reset();
        runtime.effect.reset();
        runtime.commonStates.reset();
        runtime.inputLayout.Reset();
        runtime.reticleTexture.Reset();
        runtime.reticleFailed = false;
        for (auto& state : runtime.depthReadStates) {
            state.Reset();
        }
        runtime.depthStateFailed.fill(false);
        runtime.submittedRenderTarget.Reset();
        runtime.submittedTexture = nullptr;
        runtime.submittedDescription = {};
        runtime.panelLayout = {};
    }
}
