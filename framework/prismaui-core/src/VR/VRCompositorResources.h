#pragma once

#include "VR/PresentationThreadPolicy.h"
#include "VR/VRCompositor.h"

#include <DirectXTK/CommonStates.h>
#include <DirectXTK/Effects.h>
#include <DirectXTK/PrimitiveBatch.h>
#include <DirectXTK/VertexTypes.h>

#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

namespace PrismaUI::VRCompositor
{
    namespace Resources
    {
        using Microsoft::WRL::ComPtr;

        struct PanelLayout
        {
            bool valid = false;
            RECT leftEyeRect{};
            RECT rightEyeRect{};
            UINT atlasWidth = 0;
            UINT atlasHeight = 0;
        };

        struct Runtime
        {
            ID3D11Device* device = nullptr;
            ID3D11DeviceContext* context = nullptr;
            std::unique_ptr<DirectX::CommonStates> commonStates;
            std::unique_ptr<DirectX::BasicEffect> effect;
            std::unique_ptr<
                DirectX::PrimitiveBatch<DirectX::VertexPositionColorTexture>>
                batch;
            ComPtr<ID3D11InputLayout> inputLayout;
            std::array<ComPtr<ID3D11DepthStencilState>, 9> depthReadStates{};
            std::array<bool, 9> depthStateFailed{};

            ComPtr<ID3D11ShaderResourceView> reticleTexture;
            bool reticleFailed = false;

            ID3D11Texture2D* submittedTexture = nullptr;
            ComPtr<ID3D11RenderTargetView> submittedRenderTarget;
            D3D11_TEXTURE2D_DESC submittedDescription{};
            PanelLayout panelLayout{};

            std::mutex presentationMutex;
            PresentationThreadPolicy::SerializedTracker<std::thread::id> threadTracker;
        };

        [[nodiscard]] Runtime& Get() noexcept;
        [[nodiscard]] float Width(const RECT& rectangle) noexcept;
        [[nodiscard]] float Height(const RECT& rectangle) noexcept;
        [[nodiscard]] RECT EyeRect(
            const EyeBounds& bounds,
            UINT atlasWidth,
            UINT atlasHeight) noexcept;
        [[nodiscard]] bool FiniteBounds(const EyeBounds& bounds) noexcept;
        [[nodiscard]] bool EnsureRenderer() noexcept;
    }
}
