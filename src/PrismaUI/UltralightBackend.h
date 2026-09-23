#pragma once

#include "ControllerKeyState.h"
#include "GamepadState.h"
#include "IWebBackend.h"
#include "UltralightDispatchQueue.h"
#include "UltralightDocumentCallbacks.h"
#include "UltralightInspector.h"
#include "UltralightRuntime.h"
#include "UltralightViewManager.h"
#include "UltralightViewCallbacks.h"

#include "GPU/AcceleratedDeviceLifecycle.h"
#include "GPU/FalloutD3DContext.h"

#include <d3d11.h>
#include <wrl/client.h>

#pragma warning(push)
#pragma warning(disable : 4100)
#include <Ultralight/Ultralight.h>
#pragma warning(pop)

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace PrismaUI::GPU {
    class FalloutGPUDriver;
}

namespace PrismaUI::WebRuntimeUltralight {

    class PrismaFileSystem;

    class UltralightBackend final : public PrismaUI::Web::IWebBackend,
                                    public IUltralightDocumentCallbackHost,
                                    public IUltralightInspectorHost,
                                    public IUltralightViewCallbackSink {
    public:
        UltralightBackend();
        ~UltralightBackend() override;

        void Initialize(ID3D11Device* device, ID3D11DeviceContext* immediate, HWND window,
                        PrismaUI::Web::ReadyCallback onReady) override;
        void Shutdown() override;
        bool IsReady() const override;
        void Resize(uint32_t width, uint32_t height) override;
        uint64_t DeviceEpoch() const noexcept override;
        bool PresentationStateValid() const noexcept override;
        void BeginResize(IDXGISwapChain* swapChain) override;
        void CompleteResize(IDXGISwapChain* swapChain, HRESULT result, uint32_t width, uint32_t height) override;
        PrismaUI::Web::PresentLease BeginPresentFrame(ID3D11DeviceContext* immediate,
                                                       IDXGISwapChain* swapChain) override;
        PrismaUI::Web::RenderTargetSnapshot ViewRenderTarget(
            PrismaUI::Web::ViewId view, uint64_t generation) override;

        PrismaUI::Web::ViewId CreateView(const PrismaUI::Web::ViewSource& source, const std::string& owner,
                                         PrismaUI::Web::DomReadyCallback onReady) override;
        void DestroyView(PrismaUI::Web::ViewId id) override;
        bool IsViewValid(PrismaUI::Web::ViewId id) override;
        PrismaUI::Web::ViewHealth GetViewHealth(PrismaUI::Web::ViewId id) override;
        void EnumerateViews(const PrismaUI::Web::ViewEnumCallback& callback) override;
        void FocusView(PrismaUI::Web::ViewId id) override;
        void UnfocusView(PrismaUI::Web::ViewId id) override;
        void CreateInspectorView(PrismaUI::Web::ViewId id) override;
        void SetInspectorStateCallback(PrismaUI::Web::InspectorStateCallback callback) override;
        void SetInspectorVisibility(PrismaUI::Web::ViewId id, bool visible) override;
        bool IsInspectorVisible(PrismaUI::Web::ViewId id) override;
        void SetInspectorBounds(PrismaUI::Web::ViewId id, float x, float y,
                                uint32_t width, uint32_t height) override;
        void BeginInspectorMove(PrismaUI::Web::ViewId inspectorId, int localX, int localY) override;
        void BeginInspectorResize(PrismaUI::Web::ViewId inspectorId, int localX, int localY) override;
        PrismaUI::Web::ViewId UpdateInspectorGesture(int x, int y, int& localX, int& localY) override;
        PrismaUI::Web::ViewId InspectorGestureTargetAt(int x, int y, int& localX, int& localY) override;
        void EndInspectorGesture() override;
        bool GetInspectorFrame(uint64_t generation, PrismaUI::Web::InspectorFrame& out) override;
        PrismaUI::Web::ViewId InspectorTargetAt(int x, int y, int& localX, int& localY) override;
        PrismaUI::Web::ViewId VisibleInspectorView() override;

        void ResizeView(PrismaUI::Web::ViewId id, uint32_t width, uint32_t height) override;
        void SetViewClearColor(PrismaUI::Web::ViewId id, uint32_t argb) override;
        void EvaluateScript(PrismaUI::Web::ViewId id, const std::string& script,
                            PrismaUI::Web::ScriptResultCallback callback) override;
        bool EvaluateScriptDeferred(PrismaUI::Web::ViewId id, const std::string& script,
                                    PrismaUI::Web::ScriptResultCallback callback) override;
        void RegisterLocalizationScript(PrismaUI::Web::ViewId id, const std::string& script) override;
        void CallFunction(PrismaUI::Web::ViewId id, const std::string& name,
                          const std::string& argumentJson) override;
        void RegisterListener(PrismaUI::Web::ViewId id, const std::string& name,
                              PrismaUI::Web::ListenerCallback callback) override;
        void RegisterConsole(PrismaUI::Web::ViewId id, PrismaUI::Web::ConsoleCallback callback) override;
        void SetViewErrorCallback(PrismaUI::Web::ViewErrorCallback callback) override;
        void SendKey(PrismaUI::Web::ViewId id, const PrismaUI::Web::KeyInput& input) override;
        bool SetNativeGamepad(PrismaUI::Web::ViewId id, bool enabled) override;
        bool UsesNativeGamepad(PrismaUI::Web::ViewId id) const override;
        void SendGamepad(PrismaUI::Web::ViewId id, const PrismaUI::Web::GamepadInput& input) override;
        std::uint64_t ControllerInputGeneration(PrismaUI::Web::ViewId id) const override;
        void SendControllerKey(PrismaUI::Web::ViewId id, std::uint64_t generation, std::uint32_t key,
                               bool pressed, bool fresh) override;
        void SendMouse(PrismaUI::Web::ViewId id, const PrismaUI::Web::MouseInput& input) override;
        void SendScroll(PrismaUI::Web::ViewId id, const PrismaUI::Web::ScrollInput& input) override;

    private:
        struct PublishedFrame {
            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t textureWidth = 0;
            uint32_t textureHeight = 0;
            uint32_t rowBytes = 0;
            uint32_t textureId = 0;
            std::vector<uint8_t> pixels;
            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
            float uvLeft = 0.0f;
            float uvTop = 0.0f;
            float uvRight = 1.0f;
            float uvBottom = 1.0f;
            uint64_t generation = 0;
            uint64_t contentGeneration = 0;
            uint64_t deviceEpoch = 0;
            bool carryForward = false;
            Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
            uint64_t uploadedGeneration = 0;
        };

        [[nodiscard]] PrismaUI::GPU::AcceleratedLifecycleInputs LifecycleInputs(
            ID3D11DeviceContext* immediate, IDXGISwapChain* swapChain) const noexcept;
        void InvalidateAcceleratedPresentationLocked() noexcept;
        [[nodiscard]] bool CommitAcceleratedLifecycleLocked();
        [[nodiscard]] bool ObserveAcceleratedLifecycleLocked(ID3D11DeviceContext* immediate,
                                                             IDXGISwapChain* swapChain);

        void Dispatch(UltralightDispatchQueue::Task function);
        void DispatchForView(PrismaUI::Web::ViewId id, UltralightDispatchQueue::Task function);
        bool DispatchDeferred(UltralightDispatchQueue::Task function);

        void OwnerMain(PrismaUI::Web::ReadyCallback callback);
        void PublishFrames();
        void ApplyClearColor(ViewRecord& record);

        void DeliverKey(PrismaUI::Web::ViewId id, const PrismaUI::Web::KeyInput& input);
        void ReleaseControllerKeys(PrismaUI::Web::ViewId owner, uint64_t generation);
        [[nodiscard]] bool CurrentGamepad(PrismaUI::Web::ViewId id, uint64_t generation) const;
        bool PrepareGamepad(PrismaUI::Web::ViewId id, uint64_t generation);
        void CancelNativeGamepad(PrismaUI::Web::ViewId id = 0, bool loseFocus = false);
        void PublishGamepad();
        void OnDocumentBeginLoading(PrismaUI::Web::ViewId id);
        void OnTrustedDocumentReady(PrismaUI::Web::ViewId id);
        void ReleaseNativeGamepadForInspector();
        void FocusOwnerView(PrismaUI::Web::ViewId owner);

        ultralight::RefPtr<ultralight::View> OnCreateInspectorView(
            PrismaUI::Web::ViewId owner, bool isLocal, const ultralight::String& inspectedUrl) override;
        void OnInspectorRequestClose(PrismaUI::Web::ViewId inspectorId) override;
        void NotifyInspectorState(PrismaUI::Web::ViewId owner, bool visible);

        PrismaUI::Web::ViewId NextViewId();
        bool BeginViewCreation(PrismaUI::Web::ViewId id);
        bool IsViewDestroyRequested(PrismaUI::Web::ViewId id);
        void FailViewCreation(PrismaUI::Web::ViewId id, PrismaUI::Web::DomReadyCallback onReady,
                              std::string detail);
        bool PublishCreatedView(PrismaUI::Web::ViewId id);
        void RemoveOwnerView(PrismaUI::Web::ViewId id);
        void ErasePublishedView(PrismaUI::Web::ViewId id);
        void SetHealth(ViewRecord& record, PrismaUI::Web::ViewHealth health);

        void OnConsole(PrismaUI::Web::ViewId id, const ultralight::ConsoleMessage& message) override;
        void OnChangeURL(PrismaUI::Web::ViewId id, std::string url) override;
        void OnBeginLoading(PrismaUI::Web::ViewId id, bool mainFrame, std::string url) override;
        void OnFinishLoading(PrismaUI::Web::ViewId id, bool mainFrame, std::string url) override;
        void OnFailLoading(PrismaUI::Web::ViewId id, bool mainFrame, std::string url,
                           std::string description, std::string domain, int code) override;
        void OnWindowObjectReady(PrismaUI::Web::ViewId id, bool mainFrame, std::string url) override;
        void OnDomReady(PrismaUI::Web::ViewId id, bool mainFrame, std::string url) override;
        ViewRecord* FindDocumentView(PrismaUI::Web::ViewId id) noexcept override;
        void SetDocumentHealth(ViewRecord& record, PrismaUI::Web::ViewHealth health) override;
        std::string DocumentFileUrl(std::string_view relativePath) const override;
        void ReportDocumentError(PrismaUI::Web::ViewId id, PrismaUI::Web::ViewHealth health,
                                 std::string message, std::string detail) override;
        bool DispatchDocument(std::function<void()> task) override;
        ViewRecord* FindInspectorRecord(PrismaUI::Web::ViewId id) noexcept override;
        uint32_t InspectorViewportWidth() const noexcept override;
        uint32_t InspectorViewportHeight() const noexcept override;
        void RequestInspectorFrame() noexcept override;
        void DispatchInspector(PrismaUI::Web::ViewId id, std::function<void()> task) override;
        void DispatchInspectorTask(std::function<void()> task) override;
        bool CopyPublishedInspectorTarget(PrismaUI::Web::ViewId id, uint64_t generation,
                                          PrismaUI::Web::RenderTargetSnapshot& target) const override;
        void InstallInspectorHost(ultralight::View& view, PrismaUI::Web::ViewId inspectorId) override;
        void ReportError(PrismaUI::Web::ViewId id, PrismaUI::Web::ViewHealth health,
                         std::string message, std::string detail);
        void EndPresentFrame(uint64_t generation, uint64_t epoch) noexcept override;

        mutable std::mutex stateMutex_;
        Microsoft::WRL::ComPtr<ID3D11Device> device_;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediateContext_;
        PrismaUI::GPU::FalloutD3DContext gpuContext_;
        std::unique_ptr<PrismaUI::GPU::FalloutGPUDriver> gpuDriver_;
        HWND window_ = nullptr;
        uint32_t width_ = 1280;
        uint32_t height_ = 720;
        uint64_t nextGeneration_ = 1;
        uint64_t completedGeneration_ = 0;
        uint64_t activeGeneration_ = 0;
        uint64_t deviceEpoch_ = 1;
        PrismaUI::GPU::AcceleratedDeviceLifecycle lifecycle_;
        IDXGISwapChain* resizeSwapChain_ = nullptr;
        bool initialized_ = false;
        bool acceptingFrames_ = false;
        bool ready_ = false;
        bool terminalGpuFailure_ = false;
        bool accelerated_ = false;
        std::atomic_bool ownerExited_{false};
        std::atomic_bool shutdownStarted_{false};
        std::atomic<const char*> currentOp_{"starting"};
        bool initializationDone_ = false;
        bool initializationResolved_ = false;
        bool initializationSuccess_ = false;
        std::string initializationMessage_;
        PrismaUI::Web::ViewErrorCallback errorCallback_;
        PrismaUI::Web::InspectorStateCallback inspectorStateCallback_;
        std::map<PrismaUI::Web::ViewId, PublishedFrame> published_;
        mutable std::mutex gamepadMutex_;
        PrismaUI::Web::ViewId gamepadOwner_ = 0;
        PrismaUI::Web::ViewId gamepadFocusedView_ = 0;
        uint64_t gamepadGeneration_ = 0;
        uint64_t gamepadAppliedGeneration_ = 0;
        PrismaUI::Web::ControllerKeyState controllerKeys_;
        PrismaUI::Web::ViewId controllerKeyView_ = 0;
        PrismaUI::Web::GamepadState gamepad_;
        PrismaUI::Web::GamepadState publishedGamepad_;

        UltralightRuntime runtime_;
        std::unique_ptr<PrismaFileSystem> fileSystem_;
        ultralight::RefPtr<ultralight::Renderer> renderer_;
        ultralight::RefPtr<ultralight::Session> session_;
        UltralightViewManager viewManager_;
        UltralightDocumentCallbacks documentCallbacks_;
        UltralightInspector inspector_;

        std::thread ownerThread_;
        UltralightDispatchQueue dispatchQueue_;
        std::condition_variable initializationCv_;
        std::condition_variable stateCv_;
    };

    PrismaUI::Web::IWebBackend& Backend();

}
