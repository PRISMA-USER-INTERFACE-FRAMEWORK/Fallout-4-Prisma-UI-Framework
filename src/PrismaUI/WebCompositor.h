#pragma once

#include <DirectXTK/CommonStates.h>
#include <DirectXTK/SpriteBatch.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "BackbufferOwnership.h"
#include "ModelPreview.h"
#include "RenderTargetSnapshot.h"

namespace PrismaUI {

    enum class CompositeResult {
        Success,
        NotInitialized,
        InvalidArguments,
        GetBufferFailed,
        CreateRtvFailed,
        InvalidBackbuffer,
        DrawFailed,
        SafeSkip,
    };

    inline const char* ToString(CompositeResult r) {
        switch (r) {
            case CompositeResult::Success:
                return "Success";
            case CompositeResult::NotInitialized:
                return "NotInitialized";
            case CompositeResult::InvalidArguments:
                return "InvalidArguments";
            case CompositeResult::GetBufferFailed:
                return "GetBufferFailed";
            case CompositeResult::CreateRtvFailed:
                return "CreateRtvFailed";
            case CompositeResult::InvalidBackbuffer:
                return "InvalidBackbuffer";
            case CompositeResult::DrawFailed:
                return "DrawFailed";
            case CompositeResult::SafeSkip:
                return "SafeSkip";
        }
        return "?";
    }

    class WebCompositor {
    public:
        bool Init(ID3D11Device* device, ID3D11DeviceContext* context);

        CompositeResult Draw(IDXGISwapChain* swapChain, const Web::RenderTargetSnapshot& target,
                             const std::vector<ModelPreview::Overlay>& overlays, int cursorX, int cursorY,
                             bool drawCursor);
        void InvalidateRenderTarget();

        CompositeResult ComposeAndCommitLayers(const std::vector<Web::RenderTargetSnapshot>& layers,
                                               const std::vector<ModelPreview::Overlay>& overlays, uint32_t width,
                                               uint32_t height);
        [[nodiscard]] Web::RenderTargetSnapshot CommittedSnapshot() const;
        [[nodiscard]] bool HasCommittedFrame() const noexcept { return m_committedIndex >= 0; }
        void InvalidateCommittedFrame() noexcept { m_committedIndex = -1; }

        [[nodiscard]] ID3D11Device* DevicePtr() const noexcept { return m_device.Get(); }
        [[nodiscard]] ID3D11DeviceContext* ContextPtr() const noexcept { return m_context; }

    private:
        void DrawCursor(int x, int y, bool draw);

        void DrawModelOverlays(const std::vector<ModelPreview::Overlay>& overlays);

        struct OverlaySourceSize {
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
            UINT width = 0;
            UINT height = 0;
            uint64_t lastSeenTick = 0;
        };

        struct StagingFrame {
            Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
            Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
            uint32_t width = 0;
            uint32_t height = 0;
        };
        StagingFrame m_stage[2];
        int m_committedIndex = -1;

        Microsoft::WRL::ComPtr<ID3D11Device> m_device;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vs;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> m_ps;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampler;
        Microsoft::WRL::ComPtr<ID3D11BlendState> m_blendState;
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_sourceRegion;

        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;
        D3D11_TEXTURE2D_DESC m_bbDesc{};
        IDXGISwapChain* m_swapChainIdentity = nullptr;
        bool m_backbufferBorrowed = false;
        Backbuffer::Ownership m_ownership = Backbuffer::Ownership::Unclassified;
        void* m_classifiedGetBufferFn = nullptr;

        Backbuffer::PresentationMode m_presentationMode = Backbuffer::PresentationMode::SafeSkip;
        Backbuffer::OwnershipOverride m_userOverride = Backbuffer::OwnershipOverride::Auto;

        std::unique_ptr<DirectX::SpriteBatch> m_spriteBatch;
        std::unique_ptr<DirectX::CommonStates> m_commonStates;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_cursorTex;
        bool m_cursorInitTried = false;

        std::unordered_map<ID3D11ShaderResourceView*, OverlaySourceSize> m_overlaySourceSizes;
        uint64_t m_overlayTick = 0;

        ID3D11DeviceContext* m_context = nullptr;
        bool m_initialized = false;
    };

}
