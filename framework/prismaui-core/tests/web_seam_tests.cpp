#include <cassert>
#include <utility>
#include <vector>

#include "PrismaUI/IWebBackend.h"
#include "PrismaUI/InputMessageSemantics.h"
#include "PrismaUI/InputRouting.h"

namespace {

    class FakeBackend final : public PrismaUI::Web::IWebBackend {
    public:
        using DomReadyCallback = PrismaUI::Web::DomReadyCallback;
        using ListenerCallback = PrismaUI::Web::ListenerCallback;
        using PresentLease = PrismaUI::Web::PresentLease;
        using RenderTargetSnapshot = PrismaUI::Web::RenderTargetSnapshot;
        using ScriptResultCallback = PrismaUI::Web::ScriptResultCallback;
        using ViewEnumCallback = PrismaUI::Web::ViewEnumCallback;
        using ViewErrorCallback = PrismaUI::Web::ViewErrorCallback;
        using ViewHealth = PrismaUI::Web::ViewHealth;
        using ViewId = PrismaUI::Web::ViewId;
        using ViewSource = PrismaUI::Web::ViewSource;
        using ConsoleCallback = PrismaUI::Web::ConsoleCallback;
        using KeyInput = PrismaUI::Web::KeyInput;
        using MouseInput = PrismaUI::Web::MouseInput;
        using ScrollInput = PrismaUI::Web::ScrollInput;
        using ReadyCallback = PrismaUI::Web::ReadyCallback;

        void Initialize(ID3D11Device*, ID3D11DeviceContext*, HWND, ReadyCallback) override {}
        void Shutdown() override {}
        bool IsReady() const override { return false; }
        void Resize(uint32_t, uint32_t) override {}
        bool PresentationStateValid() const noexcept override { return presentationStateValid_; }

        PresentLease BeginPresentFrame(ID3D11DeviceContext*, IDXGISwapChain* = nullptr) override {
            if (completedGeneration_ == 0 || active_) return {};
            active_ = true;
            return MakePresentLease(completedGeneration_, 1);
        }
        RenderTargetSnapshot ViewRenderTarget(ViewId, uint64_t) override { return {}; }

        ViewId CreateView(const ViewSource&, const std::string&, DomReadyCallback) override { return 0; }
        void DestroyView(ViewId) override {}
        bool IsViewValid(ViewId) override { return false; }
        ViewHealth GetViewHealth(ViewId) override { return ViewHealth::Creating; }
        void EnumerateViews(const ViewEnumCallback&) override {}
        void FocusView(ViewId) override {}
        void UnfocusView(ViewId) override {}

        void ResizeView(ViewId, uint32_t, uint32_t) override {}
        void SetViewClearColor(ViewId, uint32_t) override {}
        void EvaluateScript(ViewId, const std::string&, ScriptResultCallback) override {}
        void RegisterLocalizationScript(ViewId, const std::string&) override {}
        void CallFunction(ViewId, const std::string&, const std::string&) override {}
        void RegisterListener(ViewId, const std::string&, ListenerCallback) override {}
        void RegisterConsole(ViewId, ConsoleCallback) override {}
        void SetViewErrorCallback(ViewErrorCallback) override {}
        void SendKey(ViewId view, const KeyInput& input) override { keys.emplace_back(view, input); }
        void SendMouse(ViewId view, const MouseInput& input) override { mice.emplace_back(view, input); }
        void SendScroll(ViewId view, const ScrollInput& input) override { scrolls.emplace_back(view, input); }

        void Publish(uint64_t generation) { completedGeneration_ = generation; }
        void InvalidatePresentationState() noexcept { presentationStateValid_ = false; }

        std::vector<uint64_t> released;
        std::vector<std::pair<ViewId, KeyInput>> keys;
        std::vector<std::pair<ViewId, MouseInput>> mice;
        std::vector<std::pair<ViewId, ScrollInput>> scrolls;

    private:
        void EndPresentFrame(uint64_t generation, uint64_t) noexcept override {
            released.push_back(generation);
            active_ = false;
        }

        uint64_t completedGeneration_ = 0;
        bool active_ = false;
        bool presentationStateValid_ = true;
    };

}

int main() {
    FakeBackend backend;

    assert(!backend.BeginPresentFrame(nullptr));
    backend.Publish(1);
    auto first = backend.BeginPresentFrame(nullptr);
    assert(first);
    assert(first.generation() == 1);
    assert(!backend.BeginPresentFrame(nullptr));

    auto moved = std::move(first);
    assert(!first);
    assert(moved);
    moved = PrismaUI::Web::PresentLease{};
    assert(backend.released.size() == 1 && backend.released[0] == 1);

    {
        backend.Publish(2);
        auto second = backend.BeginPresentFrame(nullptr);
        assert(second);
        assert(second.generation() == 2);
    }
    assert(backend.released.size() == 2 && backend.released[0] == 1 && backend.released[1] == 2);

    assert(backend.PresentationStateValid());
    backend.InvalidatePresentationState();
    assert(!backend.PresentationStateValid());

    PrismaUI::Web::RenderTargetSnapshot empty;
    assert(!empty.valid());
    assert(!empty.validFor(0));
    assert(!empty.validFor(1));
    empty.publishGeneration = 1;
    assert(!empty.validFor(1));

    PrismaUI::Web::RenderTargetSnapshot padded;
    padded.viewportWidth = 1280;
    padded.viewportHeight = 720;
    padded.textureWidth = 2048;
    padded.textureHeight = 1024;
    padded.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    padded.uvLeft = 0.0f;
    padded.uvTop = 0.0f;
    padded.uvRight = 0.625f;
    padded.uvBottom = 0.703125f;
    assert(padded.hasViewportSizedUv());
    const auto upperLeft = padded.MapLogicalUv(0.0f, 0.0f);
    const auto lowerRight = padded.MapLogicalUv(1.0f, 1.0f);
    assert(upperLeft.u == padded.uvLeft && upperLeft.v == padded.uvTop);
    assert(lowerRight.u == padded.uvRight && lowerRight.v == padded.uvBottom);

    constexpr PrismaUI::Web::ViewId screenView = 42;
    static_assert(PrismaUI::WebRuntime::InputRouting::DestinationForFocusedView(screenView) == screenView);
    static_assert(PrismaUI::WebRuntime::InputRouting::HasDestination(screenView));
    static_assert(!PrismaUI::WebRuntime::InputRouting::HasDestination(0));
    PrismaUI::Web::KeyInput systemKey{PrismaUI::Web::KeyInput::Action::RawKeyDown, VK_MENU, 0, true};
    PrismaUI::WebRuntime::InputRouting::DispatchKey(backend, screenView, systemKey);
    PrismaUI::WebRuntime::InputRouting::DispatchMouse(backend, screenView, {});
    PrismaUI::WebRuntime::InputRouting::DispatchScroll(backend, screenView, {});
    PrismaUI::WebRuntime::InputRouting::DispatchKey(backend, 0, systemKey);
    PrismaUI::WebRuntime::InputRouting::DispatchMouse(backend, 0, {});
    PrismaUI::WebRuntime::InputRouting::DispatchScroll(backend, 0, {});
    assert(backend.keys.size() == 1 && backend.keys[0].first == screenView && backend.keys[0].second.systemKey);
    assert(backend.mice.size() == 1 && backend.mice[0].first == screenView);
    assert(backend.scrolls.size() == 1 && backend.scrolls[0].first == screenView);
    static_assert(!PrismaUI::WebInput::KeySemantics::IsSystemMessage(WM_KEYDOWN));
    static_assert(!PrismaUI::WebInput::KeySemantics::IsSystemMessage(WM_KEYUP));
    static_assert(!PrismaUI::WebInput::KeySemantics::IsSystemMessage(WM_CHAR));
    static_assert(PrismaUI::WebInput::KeySemantics::IsSystemMessage(WM_SYSKEYDOWN));
    static_assert(PrismaUI::WebInput::KeySemantics::IsSystemMessage(WM_SYSKEYUP));
    static_assert(PrismaUI::WebInput::KeySemantics::IsSystemMessage(WM_SYSCHAR));

    return 0;
}
