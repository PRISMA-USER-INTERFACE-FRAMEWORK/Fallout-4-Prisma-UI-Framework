#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

#include "RenderTargetSnapshot.h"
#include "GamepadState.h"

namespace PrismaUI::Web {

    using ViewId = uint64_t;

    enum class ViewHealth {
        Creating,
        DomReady,
        Live,
        LoadFailed,
        DomReadyTimeout,
        Unresponsive,
        ScriptError,
    };

    enum class MouseButton { None, Left, Middle, Right };
    enum class MouseAction { Move, Down, Up };

    struct MouseInput {
        int x = 0;
        int y = 0;
        MouseButton button = MouseButton::None;
        MouseAction action = MouseAction::Move;
        bool leaving = false;
    };

    struct ScrollInput {
        int deltaX = 0;
        int deltaY = 0;
    };

    struct KeyInput {
        enum class Action { RawKeyDown, KeyUp, Character };
        Action action = Action::RawKeyDown;
        uintptr_t wParam = 0;
        intptr_t lParam = 0;
        bool systemKey = false;
    };

    struct ViewSource {
        std::string relativePath;
    };

    struct InspectorFrame {
        RenderTargetSnapshot target;
        ViewId view = 0;
        int x = 0;
        int y = 0;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    using ReadyCallback = std::function<void(bool success, std::string message)>;
    using DomReadyCallback = std::function<void(ViewId)>;
    using ScriptResultCallback = std::function<void(std::string resultJson)>;
    using ListenerCallback = std::function<void(std::string argumentJson)>;
    using ConsoleCallback = std::function<void(int level, std::string message, std::string source, int line)>;
    using ViewErrorCallback = std::function<void(ViewId, ViewHealth, std::string message, std::string detail)>;
    using ViewEnumCallback = std::function<void(ViewId, const std::string& assetPath, const std::string& owner)>;
    using InspectorStateCallback = std::function<void(ViewId owner, bool visible)>;

    struct IWebBackend;

    class PresentLease {
    public:
        PresentLease() = default;
        PresentLease(PresentLease&& other) noexcept
            : backend_(other.backend_), generation_(other.generation_), epoch_(other.epoch_) {
            other.backend_ = nullptr;
            other.generation_ = 0;
            other.epoch_ = 0;
        }
        PresentLease& operator=(PresentLease&& other) noexcept {
            if (this != &other) {
                release();
                backend_ = other.backend_;
                generation_ = other.generation_;
                epoch_ = other.epoch_;
                other.backend_ = nullptr;
                other.generation_ = 0;
                other.epoch_ = 0;
            }
            return *this;
        }
        PresentLease(const PresentLease&) = delete;
        PresentLease& operator=(const PresentLease&) = delete;
        ~PresentLease() { release(); }

        explicit operator bool() const noexcept { return backend_ != nullptr && generation_ != 0 && epoch_ != 0; }
        [[nodiscard]] uint64_t generation() const noexcept { return generation_; }
        [[nodiscard]] uint64_t epoch() const noexcept { return epoch_; }

    private:
        friend struct IWebBackend;
        PresentLease(IWebBackend* backend, uint64_t generation, uint64_t epoch) noexcept
            : backend_(backend), generation_(generation), epoch_(epoch) {}
        void release() noexcept;
        IWebBackend* backend_ = nullptr;
        uint64_t generation_ = 0;
        uint64_t epoch_ = 0;
    };

    struct IWebBackend {
        virtual ~IWebBackend() = default;

        virtual void Initialize(ID3D11Device* device, ID3D11DeviceContext* immediate, HWND window,
                                ReadyCallback onReady) = 0;
        virtual void Shutdown() = 0;
        virtual bool IsReady() const = 0;
        virtual void Resize(uint32_t width, uint32_t height) = 0;
        virtual uint64_t DeviceEpoch() const noexcept { return 0; }
        virtual bool PresentationStateValid() const noexcept { return true; }

        virtual PresentLease BeginPresentFrame(ID3D11DeviceContext* immediate, IDXGISwapChain* swapChain = nullptr) = 0;
        virtual void BeginResize(IDXGISwapChain*) {}
        virtual void CompleteResize(IDXGISwapChain*, HRESULT, uint32_t, uint32_t) {}
        virtual RenderTargetSnapshot ViewRenderTarget(ViewId view, uint64_t generation) = 0;

        virtual ViewId CreateView(const ViewSource& source, const std::string& owner, DomReadyCallback) = 0;
        virtual void DestroyView(ViewId) = 0;
        virtual bool IsViewValid(ViewId) = 0;
        virtual ViewHealth GetViewHealth(ViewId) = 0;
        virtual void EnumerateViews(const ViewEnumCallback&) = 0;
        virtual void FocusView(ViewId) = 0;
        virtual void UnfocusView(ViewId) = 0;

        virtual void CreateInspectorView(ViewId) {}
        virtual void SetInspectorStateCallback(InspectorStateCallback) {}
        virtual void SetInspectorVisibility(ViewId, bool) {}
        virtual bool IsInspectorVisible(ViewId) { return false; }
        virtual void SetInspectorBounds(ViewId, float, float, uint32_t, uint32_t) {}
        virtual void BeginInspectorMove(ViewId, int, int) {}
        virtual void BeginInspectorResize(ViewId, int, int) {}
        virtual ViewId UpdateInspectorGesture(int, int, int&, int&) { return 0; }
        virtual ViewId InspectorGestureTargetAt(int, int, int&, int&) { return 0; }
        virtual void EndInspectorGesture() {}
        virtual bool GetInspectorFrame(uint64_t, InspectorFrame&) { return false; }
        virtual ViewId InspectorTargetAt(int, int, int&, int&) { return 0; }
        virtual ViewId VisibleInspectorView() { return 0; }

        virtual void ResizeView(ViewId, uint32_t width, uint32_t height) = 0;
        virtual void SetViewClearColor(ViewId, uint32_t argb) = 0;

        virtual void EvaluateScript(ViewId, const std::string& script, ScriptResultCallback) = 0;
        virtual bool EvaluateScriptDeferred(ViewId view, const std::string& script,
                                            ScriptResultCallback callback) {
            EvaluateScript(view, script, std::move(callback));
            return true;
        }
        virtual void RegisterLocalizationScript(ViewId, const std::string& script) = 0;
        virtual void CallFunction(ViewId, const std::string& name, const std::string& argumentJson) = 0;
        virtual void RegisterListener(ViewId, const std::string& name, ListenerCallback) = 0;
        virtual void RegisterConsole(ViewId, ConsoleCallback) = 0;
        virtual void SetViewErrorCallback(ViewErrorCallback) = 0;

        virtual void SendKey(ViewId, const KeyInput&) = 0;
        virtual bool SetNativeGamepad(ViewId, bool) { return false; }
        virtual bool UsesNativeGamepad(ViewId) const { return false; }
        virtual void SendGamepad(ViewId, const GamepadInput&) {}
        virtual std::uint64_t ControllerInputGeneration(ViewId) const { return 0; }
        virtual void SendControllerKey(ViewId, std::uint64_t, std::uint32_t, bool, bool) {}
        virtual void SendMouse(ViewId, const MouseInput&) = 0;
        virtual void SendScroll(ViewId, const ScrollInput&) = 0;

    protected:
        [[nodiscard]] PresentLease MakePresentLease(uint64_t generation, uint64_t epoch) noexcept {
            return PresentLease(this, generation, epoch);
        }
        friend class PresentLease;
        virtual void EndPresentFrame(uint64_t generation, uint64_t epoch) noexcept = 0;
    };

    inline void PresentLease::release() noexcept {
        if (backend_ != nullptr && generation_ != 0 && epoch_ != 0) backend_->EndPresentFrame(generation_, epoch_);
        backend_ = nullptr;
        generation_ = 0;
        epoch_ = 0;
    }

}
