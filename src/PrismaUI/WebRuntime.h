#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include "IWebBackend.h"
#include "InputRegionPolicy.h"
namespace PrismaUI {
    namespace WebRuntime {
        bool IsActive();
        bool WaitUntilActive(int timeoutMs);
        bool IsHostLoaded();
        bool WaitUntilHostLoaded(int timeoutMs);
        bool EnsureLoaded();
        bool LoadAndInit();
        class FrameAdvanceGuard {
        public:
            FrameAdvanceGuard() noexcept = default;
            FrameAdvanceGuard(FrameAdvanceGuard&&) noexcept = default;
            FrameAdvanceGuard& operator=(FrameAdvanceGuard&&) noexcept = default;
            FrameAdvanceGuard(const FrameAdvanceGuard&) = delete;
            FrameAdvanceGuard& operator=(const FrameAdvanceGuard&) = delete;
            [[nodiscard]] ID3D11Device* Device() const noexcept { return device_; }
            [[nodiscard]] ID3D11DeviceContext* Context() const noexcept { return context_; }
            [[nodiscard]] uint64_t PresentGeneration() const noexcept { return presentLease_.generation(); }
        private:
            friend FrameAdvanceGuard AdvanceFrame(IDXGISwapChain* swapChain);
            FrameAdvanceGuard(ID3D11Device* device, ID3D11DeviceContext* context,
                              Web::PresentLease&& presentLease) noexcept
                : device_(device), context_(context), presentLease_(std::move(presentLease)) {}
            ID3D11Device* device_ = nullptr;
            ID3D11DeviceContext* context_ = nullptr;
            Web::PresentLease presentLease_;
        };
        [[nodiscard]] FrameAdvanceGuard AdvanceFrame(IDXGISwapChain* swapChain = nullptr);
        void OnPresent(IDXGISwapChain* swapChain);
        void StartRecoveryWatchdog();
        void TryInstallGameInput();
        void OnResizeBegin(IDXGISwapChain* swapChain);
        void OnResizeComplete(IDXGISwapChain* swapChain, HRESULT result, int width, int height);
        void OnResize(int width, int height);
        void ShutdownFrameworkOnWindowThread();
        using ViewId = uint64_t;
        ViewId CreateView(const char* htmlPath, std::function<void(ViewId)> onDomReady, const char* owner = nullptr);
        void Destroy(ViewId view);
        void Show(ViewId view);
        void Hide(ViewId view);
        bool IsHidden(ViewId view);
        void SetOrder(ViewId view, int order);
        int GetOrder(ViewId view);
        bool IsValid(ViewId view);
        bool IsFrameworkSystemView(ViewId view);
        int GetViewHealth(ViewId view);
        void CreateInspectorView(ViewId view);
        void SetInspectorVisibility(ViewId view, bool visible);
        void OnInspectorBackendStateChanged(ViewId owner, bool visible);
        void RetryInspectorTeardown();
        bool IsInspectorVisible(ViewId view);
        void SetInspectorBounds(ViewId view, float topLeftX, float topLeftY, uint32_t width, uint32_t height);
        void BeginInspectorMove(ViewId view, int localX, int localY);
        void BeginInspectorResize(ViewId view, int localX, int localY);
        ViewId UpdateInspectorGesture(int x, int y, int& localX, int& localY);
        ViewId InspectorGestureTargetAt(int x, int y, int& localX, int& localY);
        void EndInspectorGesture();
        bool ToggleLocalInspector();
        void SetViewRole(ViewId view, uint32_t role);
        uint32_t GetViewRole(ViewId view);
        bool IsAnyPanelVisible(ViewId ignoreView);
        void SetViewOffscreen(ViewId view, bool offscreen);
        void SetViewOffscreenSize(ViewId view, int width, int height);
        void SetViewOffscreenBackground(ViewId view, uint32_t argb);
        bool SetViewNetworkPolicy(ViewId view, int policy);
        bool GetViewNetworkPolicy(ViewId view, int& outPolicy);
        bool SupportsViewNetworkPolicy();
        void* GetViewSRV(ViewId view);
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> AcquireViewSRV(ViewId view);
        ViewId GetFocusedView();
        bool IsOffscreen(ViewId view);
        bool GetOffscreenPageSize(ViewId view, int& outW, int& outH);
        void SetInputTargetView(ViewId view);
        bool SetNativeGamepad(ViewId view, bool enabled);
        bool UsesNativeGamepad(ViewId view);
        void SendGamepad(ViewId view, const Web::GamepadInput& input);
        std::uint64_t ControllerInputGeneration(ViewId view);
        void SendControllerKey(ViewId view, std::uint64_t generation, std::uint32_t key,
                               bool pressed, bool fresh);
        using ViewEnumFn = std::function<void(ViewId id, const std::string& htmlPath, const std::string& owner)>;
        void EnumerateViews(ViewEnumFn callback);
        void Invoke(ViewId view, const std::string& script, std::function<void(std::string)> onResult);
        void InvokeDeferred(ViewId view, const std::string& script,
                            std::function<void(std::string)> onResult);
        void RegisterLocalizationScript(ViewId view, const std::string& script);
        void InteropCall(ViewId view, const std::string& functionName, const std::string& argument);
        void RegisterJSListener(ViewId view, const std::string& fnName, std::function<void(std::string)> callback);
        void RegisterConsoleCallback(
            ViewId view, std::function<void(int level, std::string message, std::string source, int line)> callback);
        void SetViewOwnsEscape(ViewId view, bool owns);
        bool HasFocus(ViewId view);
        bool Focus(ViewId view, bool pauseGame, bool disableFocusMenu);
        bool FocusOverlay(ViewId view, bool pauseGame, bool disableFocusMenu);
        void MapClientPointToBrowser(int& x, int& y);
        bool OnWindowActivation(bool active);
        void Unfocus(ViewId view);
        bool HasAnyActiveFocus();
        bool SetInputRegions(ViewId view, const InputRegionPolicy::InputRegion* regions, uint32_t count);
        bool ShouldRouteFocusedMouse(int x, int y);
        bool IsDockCursorActive();
        void ShowBootAnimation();
        void SendMouseMove(int x, int y, uint32_t modifiers, bool mouseLeave, bool rescale = true);
        void SendMouseClick(int x, int y, int button, bool mouseUp, int clickCount, uint32_t modifiers,
                            bool rescale = true);
        void SendMouseWheel(int x, int y, int deltaX, int deltaY, uint32_t modifiers, bool rescale = true);
        void SendKey(int keyType, int windowsKeyCode, int nativeKeyCode, uint32_t modifiers, uint16_t character,
                     bool systemKey = false);
        void ShowDock();
        void HideDock();
    }
}
