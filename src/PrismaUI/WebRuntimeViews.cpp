#include "WebRuntime.h"
#include "WebRuntimeInternal.h"
#ifndef PRISMAUI_FO4VR
#include "VirtualPointer.h"
#endif
#include "GameThreadDispatcher.h"
#include "ModelPreview.h"
#include "OffscreenFrames.h"
#include "PapyrusBridge.h"
#include "TextureOverlay.h"
#include "ViewOrigin.h"
#include <string>
namespace PrismaUI::WebRuntime {
    namespace {
        auto& st = Internal::RT();
        using namespace Internal;
        std::string ResolveViewUrl(const char* htmlPath) {
            return PrismaUI::ViewOrigin::NormalizeRelativePath(htmlPath ? htmlPath : "");
        }
    }
    void EnumerateViews(ViewEnumFn callback) {
        if (!st.backend || !callback) return;
        st.backend->EnumerateViews(callback);
    }
    ViewId CreateView(const char* htmlPath, std::function<void(ViewId)> onDomReady, const char* owner) {
        if (!st.backend || !htmlPath) return 0;
        const std::string relativePath = ResolveViewUrl(htmlPath);
        if (relativePath.empty()) {
            logger::error("[WebRuntime] CreateView rejected unsafe/invalid path '{}'", htmlPath);
            return 0;
        }
        uint64_t token = 0;
        {
            std::lock_guard<std::mutex> lock(st.cbMutex);
            token = st.nextToken++;
            if (token == 0) token = st.nextToken++;
            st.domReadyByToken[token] = DomReadyRegistration{
                0, [onDomReady](ViewId id) {
                    PapyrusBridge::InjectBridge(id);
                    if (ModelPreview::Enabled()) {
                        RegisterJSListener(id, "__prismaUI_showModelPreview",
                                           [id](std::string args) { ModelPreview::Show(id, args); });
                        RegisterJSListener(id, "__prismaUI_hideModelPreview",
                                           [id](std::string args) { ModelPreview::Hide(id, args); });
                        std::string apiInit =
                            "window.__prismaUI_onModelPreviewStatus=window.__prismaUI_onModelPreviewStatus||function(){"
                            "};"
                            "window.__prismaUI_modelPreview=Object.assign(window.__prismaUI_modelPreview||{},{"
                            "version:" +
                            std::to_string(ModelPreview::kApiVersion) +
                            ","
                            "show:function(o){return window.__prismaUI_showModelPreview(JSON.stringify(o||{}));},"
                            "hide:function(o){return window.__prismaUI_hideModelPreview(JSON.stringify(o||{}));},"
                            "onStatus:function(cb){window.__prismaUI_onModelPreviewStatus="
                            "function(s){try{cb(JSON.parse(s));}catch(e){}};}"
                            "});";
                        Invoke(id, apiInit, nullptr);
                    }
                    {
                        RegisterJSListener(id, "__prismaUI_showTextureOverlay",
                                           [id](std::string args) { TextureOverlay::Show(id, args); });
                        RegisterJSListener(id, "__prismaUI_hideTextureOverlay",
                                           [id](std::string args) { TextureOverlay::Hide(id, args); });
                        std::string texInit =
                            "window.__prismaUI_onTextureOverlayStatus=window.__prismaUI_onTextureOverlayStatus||"
                            "function(){};"
                            "window.__prismaUI_textureOverlay=Object.assign(window.__prismaUI_textureOverlay||{},{"
                            "version:" +
                            std::to_string(TextureOverlay::kApiVersion) +
                            ","
                            "show:function(o){return window.__prismaUI_showTextureOverlay(JSON.stringify(o||{}));},"
                            "hide:function(o){return window.__prismaUI_hideTextureOverlay(JSON.stringify(o||{}));},"
                            "onStatus:function(cb){window.__prismaUI_onTextureOverlayStatus="
                            "function(s){try{cb(JSON.parse(s));}catch(e){}};}"
                            "});";
                        Invoke(id, texInit, nullptr);
                    }
                    if (onDomReady) onDomReady(id);
                }};
        }
        ViewId id = st.backend->CreateView(PrismaUI::Web::ViewSource{relativePath}, owner ? owner : "",
                                           [token](PrismaUI::Web::ViewId ready) { DomReadyTramp(ready, token); });
        const bool backendLive = id != 0 && st.backend->IsViewValid(id);
        bool callbackFailed = false;
        {
            std::lock_guard<std::mutex> lock(st.cbMutex);
            if (const auto failed = st.failedDomReadyTokens.find(token); failed != st.failedDomReadyTokens.end()) {
                callbackFailed = true;
                st.failedDomReadyTokens.erase(failed);
            }
            auto it = st.domReadyByToken.find(token);
            if (it != st.domReadyByToken.end()) {
                if (backendLive && !callbackFailed)
                    it->second.view = id;
                else
                    st.domReadyByToken.erase(it);
            }
            if (backendLive && !callbackFailed) {
                std::lock_guard presentationLock{st.viewPresentationMutex};
                st.viewPresentation.try_emplace(id);
            }
        }
        if (!backendLive || callbackFailed) id = 0;
        logger::info("[WebRuntime] CreateView '{}' -> path '{}' -> view {} (owner='{}')", htmlPath, relativePath, id,
                     owner ? owner : "");
        return id;
    }
    void Destroy(ViewId view) {
        if (!view) return;
        const ViewId boot = st.bootView.load(std::memory_order_acquire);
        const ViewId dock = st.dockView.load(std::memory_order_acquire);
        if (view == boot || view == dock) {
            logger::warn("[WebRuntime] refusing to destroy framework system view {}", view);
            return;
        }
#ifndef PRISMAUI_FO4VR
        GameThreadDispatcher::DropView(view);
        VirtualPointer::DiscardHeldClick(view);
#endif
        ModelPreview::OnViewDestroyed(view);
        TextureOverlay::OnViewDestroyed(view);
        if (st.focusedView.load() == view) {
            Unfocus(view);
        }
        ViewId expectedRestore = view;
        if (st.activationRestoreView.compare_exchange_strong(expectedRestore, 0, std::memory_order_acq_rel)) {
            std::lock_guard lock(st.inputRegionMutex);
            st.activationRestoreInputState = {};
        }
        if (st.localInspectorOwner.load(std::memory_order_acquire) == view ||
            st.localInspectorPending.load(std::memory_order_acquire) == view ||
            st.inspectorTeardownPending.load(std::memory_order_acquire) == view)
            SetInspectorVisibility(view, false);
        if (st.backend) st.backend->DestroyView(view);
        ForgetViewShell(view);
    }
    void ShutdownFrameworkOnWindowThread() {
        static std::atomic<bool> done{false};
        if (done.exchange(true, std::memory_order_acq_rel)) return;
        logger::info("[WebRuntime] orderly framework shutdown on window/render thread (WM_NCDESTROY)");
        const auto inspectorOwner = st.localInspectorOwner.load(std::memory_order_acquire);
        if (inspectorOwner) OnInspectorBackendStateChanged(inspectorOwner, false);
        const auto pendingTeardown = st.inspectorTeardownPending.load(std::memory_order_acquire);
        if (pendingTeardown && pendingTeardown != inspectorOwner)
            OnInspectorBackendStateChanged(pendingTeardown, false);
        st.localInspectorPending.store(0, std::memory_order_release);
        st.inspectorTeardownPending.store(0, std::memory_order_release);
        st.inspectorTeardownRetryAt.store(0, std::memory_order_release);
        st.inspectorMouseTarget.store(0, std::memory_order_release);
#ifndef PRISMAUI_FO4VR
        ModelPreview::Shutdown();
#endif
        if (st.backend) st.backend->Shutdown();
    }
    void Show(ViewId view) {
        if (!view || !IsValid(view)) return;
        std::lock_guard lock{st.viewPresentationMutex};
        const auto it = st.viewPresentation.find(view);
        if (it != st.viewPresentation.end()) it->second.visible = true;
    }
    void Hide(ViewId view) {
        {
            std::lock_guard lock{st.viewPresentationMutex};
            const auto it = st.viewPresentation.find(view);
            if (it != st.viewPresentation.end()) it->second.visible = false;
        }
        ViewId expectedRestore = view;
        st.activationRestoreView.compare_exchange_strong(expectedRestore, 0, std::memory_order_acq_rel);
        if (st.localInspectorOwner.load(std::memory_order_acquire) == view ||
            st.localInspectorPending.load(std::memory_order_acquire) == view)
            SetInspectorVisibility(view, false);
    }
    bool IsHidden(ViewId view) { return !IsRuntimeViewVisible(view); }
    void SetOrder(ViewId view, int order) {
        if (!view || !IsValid(view)) return;
        std::lock_guard lock{st.viewPresentationMutex};
        const auto it = st.viewPresentation.find(view);
        if (it != st.viewPresentation.end()) it->second.order = order;
    }
    int GetOrder(ViewId view) { return RuntimeViewOrder(view); }
    bool IsValid(ViewId view) { return st.backend ? st.backend->IsViewValid(view) : false; }
    int GetViewHealth(ViewId view) { return st.backend ? static_cast<int>(st.backend->GetViewHealth(view)) : -1; }
    void SetViewOffscreen(ViewId view, bool offscreen) {
        if (!view || !st.backend) return;
        if (offscreen && !IsValid(view)) return;
        if (offscreen && st.focusedView.load(std::memory_order_acquire) == view) {
#ifndef PRISMAUI_FO4VR
            VirtualPointer::CancelHeldClick();
#endif
        }
        if (offscreen) {
            OffscreenFrames::Track(view);
        } else {
            OffscreenFrames::Forget(view);
        }
    }
    void SetViewOffscreenSize(ViewId view, int width, int height) {
        if (!view || !IsValid(view)) return;
        if (width > 0 && height > 0) {
            std::lock_guard lock(st.offscreenSizeMutex);
            st.offscreenSizeMap[view] = {width, height};
        }
        if (st.backend && width > 0 && height > 0) st.backend->ResizeView(view, width, height);
    }
    void SetViewOffscreenBackground(ViewId view, uint32_t argb) {
        if (!view || !st.backend || !IsValid(view)) return;
        st.backend->SetViewClearColor(view, argb);
    }
    bool SupportsViewNetworkPolicy() { return false; }
    bool SetViewNetworkPolicy(ViewId view, int policy) {
        (void)view;
        (void)policy;
        return false;
    }
    bool GetViewNetworkPolicy(ViewId view, int& outPolicy) {
        (void)view;
        (void)outPolicy;
        return false;
    }
    bool GetOffscreenPageSize(ViewId view, int& outW, int& outH) {
        std::lock_guard lock(st.offscreenSizeMutex);
        auto it = st.offscreenSizeMap.find(view);
        if (it == st.offscreenSizeMap.end()) return false;
        outW = it->second.first;
        outH = it->second.second;
        return true;
    }
    bool IsOffscreen(ViewId view) { return OffscreenFrames::IsTracked(view); }
    void SetInputTargetView(ViewId view) {
        if (view && !IsValid(view)) return;
        st.inputTargetView.store(view, std::memory_order_release);
    }
    void* GetViewSRV(ViewId view) { return OffscreenFrames::PeekSRV(view); }
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> AcquireViewSRV(ViewId view) {
        return OffscreenFrames::AcquireSRV(view);
    }
    void Internal::EnsureSystemViews() {
        if (!st.backend || !st.backend->IsReady()) return;
        const auto create = [](const char* path, int order) {
            const auto view = CreateView(path, {}, "PrismaUI");
            if (!view) return PrismaUI::Web::ViewId{0};
            {
                std::lock_guard lock(st.viewPresentationMutex);
                auto it = st.viewPresentation.find(view);
                if (it != st.viewPresentation.end()) it->second.order = order;
            }
            return view;
        };
        const auto publish = [&create](std::atomic<ViewId>& slot, const char* path, int order) {
            if (slot.load(std::memory_order_acquire)) return;
            const auto view = create(path, order);
            slot.store(view, std::memory_order_release);
            if (view && !IsValid(view)) {
                ViewId expected = view;
                slot.compare_exchange_strong(expected, ViewId{0}, std::memory_order_acq_rel);
            }
        };
        publish(st.bootView, "system/boot.html", -100000);
        publish(st.dockView, "system/dock.html", 100000);
    }
    bool IsFrameworkSystemView(ViewId view) {
        if (!view) return false;
        const ViewId boot = st.bootView.load(std::memory_order_acquire);
        const ViewId dock = st.dockView.load(std::memory_order_acquire);
        if (view == boot || view == dock) return true;
        if ((boot && dock) || !st.backend) return false;
        bool system = false;
        st.backend->EnumerateViews([&](ViewId id, const std::string& path, const std::string& owner) {
            if (id == view && owner == "PrismaUI" &&
                (path == "system/boot.html" || path == "system/dock.html"))
                system = true;
        });
        return system;
    }
}
