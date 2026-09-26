#include "WebRuntime.h"
#include "WebRuntimeInternal.h"
#include <Windows.h>
#include "UltralightBackend.h"
#include "ModelPreview.h"
#include "TextureOverlay.h"
#include <string>
namespace PrismaUI::WebRuntime {
    namespace {
        auto& st = Internal::RT();
        using namespace Internal;
        bool ModelPreviewViewGate(uint64_t view) {
            return st.backend && st.backend->IsViewValid(view) && IsRuntimeViewVisible(view);
        }
        void ModelPreviewStatusSink(uint64_t viewId, const char* json) {
            if (st.backend && json) st.backend->CallFunction(viewId, "__prismaUI_onModelPreviewStatus", json);
        }
        void TextureOverlayStatusSink(uint64_t viewId, const char* json) {
            if (st.backend && json) st.backend->CallFunction(viewId, "__prismaUI_onTextureOverlayStatus", json);
        }
        void ViewErrorSink(PrismaUI::Web::ViewId view, PrismaUI::Web::ViewHealth category, std::string message,
                           std::string detail) {
            if (category == PrismaUI::Web::ViewHealth::LoadFailed && st.backend &&
                view == st.backend->VisibleInspectorView()) {
                const auto activeOwner = st.localInspectorOwner.load(std::memory_order_acquire);
                const auto pendingOwner = st.localInspectorPending.load(std::memory_order_acquire);
                const auto owner = activeOwner ? activeOwner : pendingOwner;
                if (owner && st.backend->IsInspectorVisible(owner)) SetInspectorVisibility(owner, false);
            }
            logger::error("[WebRuntime] view {} error (category={}): {} {}", view, static_cast<int>(category), message,
                          detail);
        }
        void InitResultSink(bool success, std::string message) {
            const std::string detail =
                message.empty() ? "backend returned no initialization detail" : std::move(message);
            if (success) {
                st.initFailed.store(false, std::memory_order_release);
                st.active.store(true, std::memory_order_release);
                EnsureSystemViews();
                logger::info("[WebRuntime] web backend active: {}", detail);
                return;
            }
            st.active.store(false, std::memory_order_release);
            st.initFailed.store(true, std::memory_order_release);
            logger::critical("[WebRuntime] web backend initialization failed: {}", detail);
        }
    }
    bool Internal::DoLoadAndInit() {
        st.initFailed.store(false, std::memory_order_release);
        st.active.store(false, std::memory_order_release);
        st.startupAccepted.store(false, std::memory_order_release);
        st.bootView.store(0, std::memory_order_release);
        st.dockView.store(0, std::memory_order_release);
        st.localInspectorOwner.store(0, std::memory_order_release);
        st.localInspectorPending.store(0, std::memory_order_release);
        st.inspectorTeardownPending.store(0, std::memory_order_release);
        st.inspectorTeardownRetryAt.store(0, std::memory_order_release);
        st.inspectorMouseTarget.store(0, std::memory_order_release);
        st.inspectorPointerCaptureTarget.store(0, std::memory_order_release);
        st.inspectorPointerOriginX.store(0, std::memory_order_release);
        st.inspectorPointerOriginY.store(0, std::memory_order_release);
        st.backend = &PrismaUI::WebRuntimeUltralight::Backend();
        st.backend->SetInspectorStateCallback(
            [](PrismaUI::Web::ViewId owner, bool visible) { OnInspectorBackendStateChanged(owner, visible); });
        st.backendReady.store(true);
        logger::info("[WebRuntime] in-process Ultralight CPU backend acquired");
        auto* rendererData = RE::BSGraphics::GetRendererData();
        if (!rendererData) {
            logger::critical("[WebRuntime] BSGraphics::GetRendererData() null");
            st.initFailed.store(true, std::memory_order_release);
            return false;
        }
        auto* device = reinterpret_cast<ID3D11Device*>(rendererData->device);
        auto hwnd = reinterpret_cast<HWND>(rendererData->renderWindow[0].hwnd);
        if (!device || !hwnd) {
            logger::critical("[WebRuntime] null device/HWND from BSGraphics");
            st.initFailed.store(true, std::memory_order_release);
            return false;
        }
        st.immediateContext.Reset();
        device->GetImmediateContext(&st.immediateContext);
        ID3D11DeviceContext* context = st.immediateContext.Get();
        if (!context) {
            logger::critical("[WebRuntime] device->GetImmediateContext() returned null");
            st.initFailed.store(true, std::memory_order_release);
            return false;
        }
        auto* rdContext = reinterpret_cast<ID3D11DeviceContext*>(rendererData->context);
        if (rdContext != context) {
            logger::warn(
                "[WebRuntime] RendererData::context ({}) != device immediate context ({}) -- an "
                "upscaler/proxy likely wrapped RendererData::context; using device immediate "
                "context (unproven vs the proxy -- watch for state-tracking issues)",
                static_cast<void*>(rdContext), static_cast<void*>(context));
        } else {
            logger::info("[WebRuntime] device immediate context matches RendererData::context");
        }
        st.device.store(device);
        st.context.store(context);
#if defined(PRISMAUI_FO4VR)
#else
        if (!st.compositor.Init(device, context)) {
            logger::critical("[WebRuntime] WebCompositor::Init failed");
            st.initFailed.store(true, std::memory_order_release);
            return false;
        }
        st.compositorReady.store(true);
#endif
        ModelPreview::SetViewGate(&ModelPreviewViewGate);
        ModelPreview::SetStatusSink(&ModelPreviewStatusSink);
        TextureOverlay::SetStatusSink(&TextureOverlayStatusSink);
        st.inputHwnd.store(hwnd);
        if (auto* task = F4SE::GetTaskInterface()) {
            task->AddTask([] { TryInstallGameInput(); });
        }
        st.backend->SetViewErrorCallback(&ViewErrorSink);
        st.backend->Initialize(device, context, hwnd, &InitResultSink);
        st.startupAccepted.store(true, std::memory_order_release);
        logger::info("[WebRuntime] web backend startup accepted; awaiting terminal init result");
        return true;
    }
}
