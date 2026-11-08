#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>

namespace PrismaUI::SceneDepthCapture::Internal
{
    using CommitGraphicsState =
        void (*)(std::uint8_t, std::uint8_t);

    struct Capture
    {
        Microsoft::WRL::ComPtr<IUnknown> colorIdentity;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView>
            originalView;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView>
            readOnlyView;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        D3D11_TEXTURE2D_DESC textureDescription{};
        D3D11_DEPTH_STENCIL_VIEW_DESC viewDescription{};
        D3D11_COMPARISON_FUNC comparison =
            D3D11_COMPARISON_LESS_EQUAL;
        std::uint64_t frameEpoch = 0;
    };

    inline std::mutex g_captureMutex;
    inline std::mutex g_targetsMutex;
    inline std::atomic<bool> g_requested = false;
    inline std::atomic<std::uint64_t> g_frameEpoch = 1;
    inline std::atomic<std::uint64_t> g_capturedEpoch = 0;
    inline CommitGraphicsState g_original = nullptr;
    inline Capture g_capture;
    inline std::array<Microsoft::WRL::ComPtr<IUnknown>, 3>
        g_submittedTargets{};

    void HookCommit(
        std::uint8_t firstMode,
        std::uint8_t secondMode) noexcept;
}
