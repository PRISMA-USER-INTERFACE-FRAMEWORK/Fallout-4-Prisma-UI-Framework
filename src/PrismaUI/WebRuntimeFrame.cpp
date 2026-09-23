#include "WebRuntime.h"
#include "WebRuntimeInternal.h"
#include "Menus/FocusMenu/FocusMenu.h"
#include "OffscreenFrames.h"
#include "PresentProfiler.h"
namespace PrismaUI::WebRuntime {
    namespace {
        auto& st = Internal::RT();
        using namespace Internal;
    }
    FrameAdvanceGuard AdvanceFrame(IDXGISwapChain* swapChain) {
        TickPauseMaintenance();
        FocusMenu::Tick();
        ID3D11Device* dev = st.device.load();
        ID3D11DeviceContext* ctx = st.context.load();
        if (!st.backend || !dev || !ctx) return {};
        const auto beforeEpoch = st.backend->DeviceEpoch();
        auto presentLease = st.backend->BeginPresentFrame(ctx, swapChain);
        if (beforeEpoch != st.backend->DeviceEpoch() || !st.backend->PresentationStateValid()) {
            ClearHeldPresentState();
            st.compositor.InvalidateRenderTarget();
            st.compositor.InvalidateCommittedFrame();
            st.srvPresent.store(false, std::memory_order_release);
        }
        if (!presentLease) return {};
        PrismaUI::PresentProfiler::BeginStage(PrismaUI::PresentProfiler::Stage::UpdateOffscreen, dev, ctx);
        OffscreenFrames::Update(st.backend, presentLease.generation(), dev, ctx);
        PrismaUI::PresentProfiler::EndStage(PrismaUI::PresentProfiler::Stage::UpdateOffscreen, ctx);
        return FrameAdvanceGuard(dev, ctx, std::move(presentLease));
    }
    void OnResizeBegin(IDXGISwapChain* swapChain) {
        if (!st.backend) return;
        EndInspectorGesture();
        st.backend->BeginResize(swapChain);
        st.compositor.InvalidateRenderTarget();
        ClearHeldPresentState();
        st.compositor.InvalidateCommittedFrame();
        st.srvPresent.store(false, std::memory_order_release);
    }
    void OnResizeComplete(IDXGISwapChain* swapChain, HRESULT result, int width, int height) {
        if (!st.backend) return;
        st.backend->CompleteResize(swapChain, result, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        if (SUCCEEDED(result) && width > 0 && height > 0) st.backend->Resize(width, height);
    }
    void OnResize(int width, int height) {
        if (!st.backend) return;
        EndInspectorGesture();
        st.compositor.InvalidateRenderTarget();
        if (width > 0 && height > 0) {
            st.backend->Resize(width, height);
        }
    }
}
