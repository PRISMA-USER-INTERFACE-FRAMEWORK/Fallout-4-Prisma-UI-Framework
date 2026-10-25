#pragma once

#include "VR/SceneDepthCapture.h"
#include "VR/SpatialPointer.h"
#include "VR/StereoProjection.h"
#include "VR/WorldPanelGeometry.h"

#include <DirectXTK/VertexTypes.h>

#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <d3d11.h>

namespace PrismaUI::VRCompositor
{
    namespace Draw
    {
        using Microsoft::WRL::ComPtr;

        struct ResolvedTarget
        {
            ComPtr<ID3D11ShaderResourceView> textureOwned;
            ID3D11ShaderResourceView* texture = nullptr;
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            float u0 = 0.0f;
            float v0 = 0.0f;
            float u1 = 1.0f;
            float v1 = 1.0f;

            [[nodiscard]] bool IsValid() const noexcept
            {
                return texture && width > 0 && height > 0;
            }
        };

        [[nodiscard]] DirectX::VertexPositionColorTexture Vertex(
            const WorldPanelGeometry::Vec3& point,
            float u,
            float v,
            float opacity = 1.0f) noexcept;
        [[nodiscard]] bool DrawWorldQuad(
            const ResolvedTarget& target,
            const std::array<DirectX::VertexPositionColorTexture, 4>& vertices,
            const StereoProjection::Snapshot& projection,
            bool useDepth,
            const SceneDepthCapture::FrameDepth* sceneDepth) noexcept;
        [[nodiscard]] bool DrawReticle(
            const SpatialPointer::ReticleSnapshot& reticle,
            const StereoProjection::Snapshot& projection,
            bool useDepth,
            const SceneDepthCapture::FrameDepth* sceneDepth) noexcept;
    }
}
