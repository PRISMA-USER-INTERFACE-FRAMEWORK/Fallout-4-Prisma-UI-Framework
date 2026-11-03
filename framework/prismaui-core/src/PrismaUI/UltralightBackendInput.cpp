#include "UltralightBackend.h"

#include "ClearColorPolicy.h"
#include "ControllerActions.h"
#include "InvokeResult.h"
#include "OnceCompletion.h"
#include "UltralightGamepad.h"
#include "UltralightString.h"
#include "Utf16Pairing.h"

#include <AppCore/JSHelpers.h>

#include <cassert>
#include <memory>
#include <utility>

namespace PrismaUI::WebRuntimeUltralight {

    namespace {
        using ScriptCompletion = OnceCompletion<std::string>;
        std::string ScriptStopped() {
            return PrismaUI::InvokeResult::Error("web backend stopped before the script ran");
        }
    }

    void UltralightBackend::ResizeView(PrismaUI::Web::ViewId id, uint32_t width, uint32_t height) {
        if (!width || !height) return;
        NoteStructuralChange();
        DispatchForView(id, [this, id, width, height] {
            NoteStructuralChange();
            if (auto* record = viewManager_.Find(id)) {
                record->viewportSized = false;
                record->width = width;
                record->height = height;
                if (record->view) record->view->Resize(width, height);
            }
        });
    }

    void UltralightBackend::SetViewClearColor(PrismaUI::Web::ViewId id, uint32_t argb) {
        DispatchForView(id, [this, id, argb] {
            auto* record = viewManager_.Find(id);
            if (!record) return;
            record->clearColor = argb;
            if (!record->view || !record->trustedDocumentReady) return;
            const auto script = ClearColorPolicy::BackgroundScript(argb);
            ultralight::String exception;
            record->view->EvaluateScript(ultralight::String(script.c_str()), &exception);
            if (!exception.empty()) {
                ReportError(id, PrismaUI::Web::ViewHealth::ScriptError,
                            "JavaScript evaluation failed", ToUtf8(exception));
            }
        });
    }

    void UltralightBackend::EvaluateScript(PrismaUI::Web::ViewId id, const std::string& script,
                                           PrismaUI::Web::ScriptResultCallback callback) {
        auto done = callback ? std::make_shared<ScriptCompletion>(std::move(callback), ScriptStopped()) : nullptr;
        DispatchForView(id, [this, id, script, done] {
            auto* record = viewManager_.Find(id);
            if (!record || !record->view) {
                if (done) done->Complete("{\"error\":\"Invoke target view is not live\"}");
                return;
            }
            ultralight::String exception;
            const auto result = record->view->EvaluateScript(ultralight::String(script.c_str()), &exception);
            if (!exception.empty()) {
                ReportError(id, PrismaUI::Web::ViewHealth::ScriptError,
                            "JavaScript evaluation failed", ToUtf8(exception));
            }
            if (done) {
                done->Complete(exception.empty() ? ToUtf8(result) : PrismaUI::InvokeResult::Error(ToUtf8(exception)));
            }
        });
    }

    bool UltralightBackend::EvaluateScriptDeferred(PrismaUI::Web::ViewId id,
                                                   const std::string& script,
                                                   PrismaUI::Web::ScriptResultCallback callback) {
        auto done = callback ? std::make_shared<ScriptCompletion>(std::move(callback), ScriptStopped()) : nullptr;
        return DispatchDeferred([this, id, script, done] {
            auto* record = viewManager_.Find(id);
            if (!record || !record->view) {
                if (done) done->Complete("{\"error\":\"Invoke target view is not live\"}");
                return;
            }
            ultralight::String exception;
            const auto result = record->view->EvaluateScript(ultralight::String(script.c_str()), &exception);
            if (!exception.empty()) {
                ReportError(id, PrismaUI::Web::ViewHealth::ScriptError,
                            "JavaScript evaluation failed", ToUtf8(exception));
            }
            if (done) {
                done->Complete(exception.empty() ? ToUtf8(result) : PrismaUI::InvokeResult::Error(ToUtf8(exception)));
            }
        });
    }

    void UltralightBackend::RegisterLocalizationScript(PrismaUI::Web::ViewId id, const std::string& script) {
        DispatchForView(id, [this, id, script] {
            auto* record = viewManager_.Find(id);
            if (!record || !record->view) return;
            record->localizationScript = script;
            if (!record->trustedDocumentReady) return;
            ultralight::String exception;
            record->view->EvaluateScript(ultralight::String(script.c_str()), &exception);
            if (!exception.empty()) {
                ReportError(id, PrismaUI::Web::ViewHealth::ScriptError,
                            "JavaScript evaluation failed", ToUtf8(exception));
            }
        });
    }

    void UltralightBackend::CallFunction(PrismaUI::Web::ViewId id, const std::string& name,
                                         const std::string& argumentJson) {
        DispatchForView(id, [this, id, name, argumentJson] {
            auto* stored = viewManager_.Find(id);
            if (!stored || !stored->view || name.empty()) return;
            auto& record = *stored;
            if (!record.documentDomReady || record.replayingPendingCalls) {
                constexpr std::size_t kMaxPendingCallBytes = 1u << 20;
                const std::size_t bytes = name.size() + argumentJson.size();
                if (bytes > kMaxPendingCallBytes) return;
                const bool coalesce = name == "__prismaUI_onModelPreviewStatus" ||
                                      name == "__prismaUI_onTextureOverlayStatus";
                if (coalesce) {
                    for (auto pending = record.pendingCalls.begin(); pending != record.pendingCalls.end(); ++pending) {
                        if (pending->name != name) continue;
                        record.pendingCallBytes -= pending->name.size() + pending->argumentJson.size();
                        record.pendingCalls.erase(pending);
                        break;
                    }
                }
                while (!record.pendingCalls.empty() &&
                       (record.pendingCalls.size() >= 128 ||
                        record.pendingCallBytes + bytes > kMaxPendingCallBytes)) {
                    record.pendingCallBytes -= record.pendingCalls.front().name.size() +
                                               record.pendingCalls.front().argumentJson.size();
                    record.pendingCalls.pop_front();
                }
                record.pendingCalls.push_back({name, argumentJson});
                record.pendingCallBytes += bytes;
                return;
            }
            documentCallbacks_.InvokeFunction(record, name, argumentJson);
        });
    }

    void UltralightBackend::RegisterListener(PrismaUI::Web::ViewId id, const std::string& name,
                                             PrismaUI::Web::ListenerCallback callback) {
        if (name.empty() || name.size() > 256) return;
        DispatchForView(id, [this, id, name, callback = std::move(callback)] {
            auto* record = viewManager_.Find(id);
            if (!record || !record->view) return;
            if (callback) record->listeners[name] = callback;
            else record->listeners.erase(name);
            documentCallbacks_.InstallListener(*record, name);
        });
    }

    void UltralightBackend::RegisterConsole(PrismaUI::Web::ViewId id,
                                            PrismaUI::Web::ConsoleCallback callback) {
        DispatchForView(id, [this, id, callback = std::move(callback)] {
            if (auto* record = viewManager_.Find(id)) record->console = std::move(callback);
        });
    }

    void UltralightBackend::SetViewErrorCallback(PrismaUI::Web::ViewErrorCallback callback) {
        std::lock_guard lock(stateMutex_);
        errorCallback_ = std::move(callback);
    }

    void UltralightBackend::SetDocumentReadyCallback(PrismaUI::Web::DomReadyCallback callback) {
        std::lock_guard lock(stateMutex_);
        documentReadyCallback_ = std::move(callback);
    }

    void UltralightBackend::SendKey(PrismaUI::Web::ViewId id, const PrismaUI::Web::KeyInput& input) {
        DispatchForView(id, [this, id, input] { DeliverKey(id, input); });
    }

    void UltralightBackend::DeliverKey(PrismaUI::Web::ViewId id, const PrismaUI::Web::KeyInput& input) {
        assert(dispatchQueue_.IsOwnerThread());
        auto* record = viewManager_.Find(id);
        if (!record || !record->view) return;
        auto type = ultralight::KeyEvent::kType_RawKeyDown;
        if (input.action == PrismaUI::Web::KeyInput::Action::KeyUp) type = ultralight::KeyEvent::kType_KeyUp;
        if (input.action == PrismaUI::Web::KeyInput::Action::Character) type = ultralight::KeyEvent::kType_Char;
        auto event = ultralight::KeyEvent(type, input.wParam, input.lParam, input.systemKey);
        if (type == ultralight::KeyEvent::kType_Char && input.wParam > 0xFFFF) {
            const auto pair = Utf16Pairing::ToSurrogatePair(static_cast<char32_t>(input.wParam));
            const ultralight::Char16 units[] = {static_cast<ultralight::Char16>(pair[0]),
                                                static_cast<ultralight::Char16>(pair[1])};
            event.text = ultralight::String(units, 2);
            event.unmodified_text = ultralight::String(units, 2);
        }
        if (type == ultralight::KeyEvent::kType_Char && input.wParam == VK_RETURN) {
            event.text = ultralight::String("\n");
            event.unmodified_text = ultralight::String("\n");
        }
        record->view->FireKeyEvent(event);
    }

    std::uint64_t UltralightBackend::ControllerInputGeneration(PrismaUI::Web::ViewId id) const {
        std::lock_guard lock(gamepadMutex_);
        return id && gamepadFocusedView_ == id && !gamepadOwner_ ? gamepadGeneration_ : 0;
    }

    void UltralightBackend::SendControllerKey(PrismaUI::Web::ViewId id, std::uint64_t generation,
                                              std::uint32_t key, bool pressed, bool fresh) {
        (void)DispatchDeferred([this, id, generation, key, pressed, fresh] {
            if (ControllerInputGeneration(id) != generation || !PrepareGamepad(0, generation)) return;
            if (ControllerInputGeneration(id) != generation || !controllerKeys_.Apply(key, pressed, fresh)) return;
            controllerKeyView_ = id;
            DeliverKey(id, {pressed ? PrismaUI::Web::KeyInput::Action::RawKeyDown
                                    : PrismaUI::Web::KeyInput::Action::KeyUp,
                            key, 0, false});
        });
    }

    bool UltralightBackend::SetNativeGamepad(PrismaUI::Web::ViewId id, bool enabled) {
        if (!IsViewValid(id)) return false;
        if (!enabled) {
            CancelNativeGamepad(id);
            return true;
        }
        std::uint64_t generation;
        {
            std::lock_guard lock(gamepadMutex_);
            if (gamepadFocusedView_ != id) return false;
            if (gamepadOwner_ == id) return true;
            gamepadOwner_ = id;
            generation = ++gamepadGeneration_;
        }
        if (DispatchDeferred([this, id, generation] { PrepareGamepad(id, generation); })) return true;
        CancelNativeGamepad(id);
        return false;
    }

    bool UltralightBackend::UsesNativeGamepad(PrismaUI::Web::ViewId id) const {
        std::lock_guard lock(gamepadMutex_);
        return id && gamepadOwner_ == id;
    }

    void UltralightBackend::SendGamepad(PrismaUI::Web::ViewId id, const PrismaUI::Web::GamepadInput& input) {
        std::uint64_t generation;
        {
            std::lock_guard lock(gamepadMutex_);
            if (!id || gamepadOwner_ != id) return;
            generation = gamepadGeneration_;
        }
        (void)DispatchDeferred([this, id, generation, input] {
            if (!PrepareGamepad(id, generation)) return;
            gamepad_.Apply(input);
            PublishGamepad();
        });
    }

    void UltralightBackend::ReleaseControllerKeys(PrismaUI::Web::ViewId owner, uint64_t generation) {
        assert(dispatchQueue_.IsOwnerThread());
        const auto view = controllerKeyView_;
        const auto* record = viewManager_.Find(view);
        const auto document = record ? record->documentGeneration : 0;
        controllerKeyView_ = 0;
        controllerKeys_.Release([&](std::uint32_t key) {
            if (!CurrentGamepad(owner, generation)) return false;
            const auto* current = viewManager_.Find(view);
            if (!current || current->documentGeneration != document) return false;
            DeliverKey(view, {PrismaUI::Web::KeyInput::Action::KeyUp, key, 0, false});
            return true;
        });
    }

    bool UltralightBackend::CurrentGamepad(PrismaUI::Web::ViewId id, uint64_t generation) const {
        std::lock_guard lock(gamepadMutex_);
        return gamepadOwner_ == id && gamepadGeneration_ == generation;
    }

    bool UltralightBackend::PrepareGamepad(PrismaUI::Web::ViewId id, uint64_t generation) {
        assert(dispatchQueue_.IsOwnerThread());
        if (!CurrentGamepad(id, generation)) return false;
        if (gamepadAppliedGeneration_ != generation) {
            ReleaseControllerKeys(id, generation);
            if (!CurrentGamepad(id, generation)) return false;
            gamepad_ = {};
            PublishGamepad();
            gamepadAppliedGeneration_ = generation;
        }
        return true;
    }

    void UltralightBackend::CancelNativeGamepad(PrismaUI::Web::ViewId id, bool loseFocus) {
        std::uint64_t generation;
        {
            std::lock_guard lock(gamepadMutex_);
            if (id && gamepadFocusedView_ != id && gamepadOwner_ != id) return;
            if (loseFocus) gamepadFocusedView_ = 0;
            gamepadOwner_ = 0;
            generation = ++gamepadGeneration_;
        }
        (void)DispatchDeferred([this, generation] { PrepareGamepad(0, generation); });
    }

    void UltralightBackend::PublishGamepad() {
        assert(dispatchQueue_.IsOwnerThread());
        if (renderer_)
            ::PrismaUI::Web::PublishGamepad(*renderer_, gamepad_, publishedGamepad_);
    }

    void UltralightBackend::SendMouse(PrismaUI::Web::ViewId id, const PrismaUI::Web::MouseInput& input) {
        DispatchForView(id, [this, id, input] {
            auto* record = viewManager_.Find(id);
            if (!record || !record->view) return;
            ultralight::MouseEvent event{};
            event.type = input.action == PrismaUI::Web::MouseAction::Down
                             ? ultralight::MouseEvent::kType_MouseDown
                             : input.action == PrismaUI::Web::MouseAction::Up
                                   ? ultralight::MouseEvent::kType_MouseUp
                                   : ultralight::MouseEvent::kType_MouseMoved;
            event.x = input.x;
            event.y = input.y;
            event.button = input.button == PrismaUI::Web::MouseButton::Left
                               ? ultralight::MouseEvent::kButton_Left
                               : input.button == PrismaUI::Web::MouseButton::Middle
                                     ? ultralight::MouseEvent::kButton_Middle
                                     : input.button == PrismaUI::Web::MouseButton::Right
                                           ? ultralight::MouseEvent::kButton_Right
                                           : ultralight::MouseEvent::kButton_None;
            record->view->FireMouseEvent(event);
        });
    }

    void UltralightBackend::SendScroll(PrismaUI::Web::ViewId id, const PrismaUI::Web::ScrollInput& input) {
        DispatchForView(id, [this, id, input] {
            auto* record = viewManager_.Find(id);
            if (!record || !record->view) return;
            ultralight::ScrollEvent event{};
            event.type = ultralight::ScrollEvent::kType_ScrollByPixel;
            event.delta_x = input.deltaX;
            event.delta_y = input.deltaY;
            record->view->FireScrollEvent(event);
        });
    }

    bool UltralightBackend::PageHasInputFocus() const noexcept {
        return inputFocusView_.load(std::memory_order_acquire) != 0;
    }

    void UltralightBackend::PublishInputFocus() {
        assert(dispatchQueue_.IsOwnerThread());
        PrismaUI::Web::ViewId editing = 0;
        viewManager_.ForEach([&editing](PrismaUI::Web::ViewId id, ViewRecord& record) {
            if (!editing && record.view && record.view->HasFocus() && record.view->HasInputFocus()) editing = id;
        });
        const auto previous = inputFocusView_.exchange(editing, std::memory_order_release);
        if (previous != editing && window_) PostMessageW(window_, WM_NULL, 0, 0);
    }

    void UltralightBackend::ClearInputFocus(PrismaUI::Web::ViewId id) {
        PrismaUI::Web::ViewId expected = id;
        if (inputFocusView_.compare_exchange_strong(expected, 0, std::memory_order_release,
                                                    std::memory_order_relaxed) &&
            window_)
            PostMessageW(window_, WM_NULL, 0, 0);
    }

}
