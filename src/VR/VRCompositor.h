#pragma once

#include "VR/SceneDepthCapture.h"

#include <d3d11.h>

#include <cstdint>

namespace PrismaUI::VRCompositor
{

    struct EyeBounds
    {
        bool valid = false;
        float uMin = 0.0f;
        float vMin = 0.0f;
        float uMax = 0.0f;
        float vMax = 0.0f;
    };

    struct SubmittedTextureLayout
    {
        EyeBounds left{};
        EyeBounds right{};
        bool stereoPairVerified = false;
    };

    void RenderSubmittedTexture(
        ID3D11Texture2D* texture,
        const SubmittedTextureLayout& layout) noexcept;

    void OnResizeBuffers() noexcept;
    void ReleaseDeviceResources() noexcept;
    void Shutdown() noexcept;

    [[nodiscard]] bool IsOperational() noexcept;
}
