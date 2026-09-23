#include "UltralightInspector.h"

#include <AppCore/JSHelpers.h>

#include <algorithm>
#include <utility>

namespace PrismaUI::WebRuntimeUltralight {

    namespace {

        constexpr uint32_t kInspectorMinimumDimension = 32;

    }

    UltralightInspector::UltralightInspector(IUltralightInspectorHost& host) : host_(host) {}

    void UltralightInspector::Add(
        PrismaUI::Web::ViewId owner, PrismaUI::Web::ViewId id,
        ultralight::RefPtr<ultralight::View> view, uint32_t width, uint32_t height, int x, int y) {
        InspectorState state;
        state.id = id;
        state.view = std::move(view);
        state.width = width;
        state.height = height;
        state.x = x;
        state.y = y;
        std::lock_guard lock(mutex_);
        inspectors_[owner] = std::move(state);
    }

    void UltralightInspector::Clear() noexcept {
        std::lock_guard lock(mutex_);
        inspectors_.clear();
    }

    void UltralightInspector::RemoveForOwner(PrismaUI::Web::ViewId owner) {
        bool wasVisible = false;
        {
            std::lock_guard lock(mutex_);
            const auto it = inspectors_.find(owner);
            if (it == inspectors_.end()) return;
            wasVisible = it->second.visible;
            inspectors_.erase(it);
        }
        if (wasVisible) host_.NotifyInspectorState(owner, false);
    }

    void UltralightInspector::OnInspectorRequestClose(PrismaUI::Web::ViewId inspectorId) {
        PrismaUI::Web::ViewId owner = 0;
        ultralight::RefPtr<ultralight::View> inspector;
        ultralight::RefPtr<ultralight::View> ownerView;
        {
            std::lock_guard lock(mutex_);
            for (auto& [candidate, state] : inspectors_) {
                if (state.id != inspectorId) continue;
                owner = candidate;
                state.visible = false;
                state.gesture = InspectorState::Gesture::None;
                inspector = state.view;
                if (const auto record = host_.FindInspectorRecord(owner)) ownerView = record->view;
                break;
            }
        }
        if (!owner) return;
        if (inspector) inspector->Unfocus();
        if (ownerView) ownerView->Focus();
        host_.NotifyInspectorState(owner, false);
    }

    void UltralightInspector::SetViewport(uint32_t width, uint32_t height) {
        std::lock_guard lock(mutex_);
        for (auto& [_, state] : inspectors_) {
            if (!state.view) continue;
            const uint32_t minimumWidth = std::min(kInspectorMinimumDimension, width);
            const uint32_t minimumHeight = std::min(kInspectorMinimumDimension, height);
            state.width = std::clamp(state.width, minimumWidth, width);
            state.height = std::clamp(state.height, minimumHeight, height);
            state.x = std::clamp(state.x, 0, static_cast<int>(width - state.width));
            state.y = std::clamp(state.y, 0, static_cast<int>(height - state.height));
            state.view->Resize(state.width, state.height);
        }
    }

    void UltralightInspector::SetVisibility(PrismaUI::Web::ViewId owner, bool visible) {
        host_.DispatchInspector(owner, [this, owner, visible] {
            auto* record = host_.FindInspectorRecord(owner);
            if (!record || record->isInspector || !record->view) {
                host_.NotifyInspectorState(owner, false);
                return;
            }
            if (visible && !record->inspectorId) {
                try {
                    record->view->CreateLocalInspectorView();
                } catch (...) {
                    host_.NotifyInspectorState(owner, false);
                    return;
                }
            }
            ultralight::RefPtr<ultralight::View> inspector;
            bool missing = false;
            {
                std::lock_guard lock(mutex_);
                const auto state = inspectors_.find(owner);
                if (state == inspectors_.end() || !state->second.view) {
                    missing = true;
                } else {
                    state->second.visible = visible;
                    if (!visible) state->second.gesture = InspectorState::Gesture::None;
                    inspector = state->second.view;
                }
            }
            if (missing) {
                host_.NotifyInspectorState(owner, false);
                return;
            }
            if (visible) {
                host_.ReleaseNativeGamepadForInspector();
                record->view->Unfocus();
                inspector->Focus();
            } else {
                inspector->Unfocus();
                host_.FocusOwnerView(owner);
            }
            host_.NotifyInspectorState(owner, visible);
        });
    }

    bool UltralightInspector::IsVisible(PrismaUI::Web::ViewId owner) const {
        std::lock_guard lock(mutex_);
        const auto it = inspectors_.find(owner);
        return it != inspectors_.end() && it->second.visible;
    }

    void UltralightInspector::SetBounds(
        PrismaUI::Web::ViewId owner, float x, float y, uint32_t width, uint32_t height) {
        if (!width || !height) return;
        host_.DispatchInspector(owner, [this, owner, x, y, width, height] {
            ultralight::RefPtr<ultralight::View> inspector;
            uint32_t inspectorWidth = 0;
            uint32_t inspectorHeight = 0;
            {
                std::lock_guard lock(mutex_);
                const auto it = inspectors_.find(owner);
                if (it == inspectors_.end() || !it->second.view) return;
                auto& state = it->second;
                const auto viewportWidth = host_.InspectorViewportWidth();
                const auto viewportHeight = host_.InspectorViewportHeight();
                const uint32_t minimumWidth = std::min(kInspectorMinimumDimension, viewportWidth);
                const uint32_t minimumHeight = std::min(kInspectorMinimumDimension, viewportHeight);
                const uint32_t nextWidth = std::min(width, viewportWidth);
                const uint32_t nextHeight = std::min(height, viewportHeight);
                if (nextWidth < minimumWidth || nextHeight < minimumHeight) return;
                state.width = nextWidth;
                state.height = nextHeight;
                state.x = std::clamp(static_cast<int>(x), 0, static_cast<int>(viewportWidth - state.width));
                state.y = std::clamp(static_cast<int>(y), 0, static_cast<int>(viewportHeight - state.height));
                inspector = state.view;
                inspectorWidth = state.width;
                inspectorHeight = state.height;
            }
            inspector->Resize(inspectorWidth, inspectorHeight);
        });
    }

    void UltralightInspector::BeginMove(
        PrismaUI::Web::ViewId inspectorId, int localX, int localY) {
        BeginGesture(inspectorId, localX, localY, InspectorState::Gesture::Move);
    }

    void UltralightInspector::BeginResize(
        PrismaUI::Web::ViewId inspectorId, int localX, int localY) {
        BeginGesture(inspectorId, localX, localY, InspectorState::Gesture::Resize);
    }

    PrismaUI::Web::ViewId UltralightInspector::UpdateGesture(
        int x, int y, int& localX, int& localY) {
        ultralight::RefPtr<ultralight::View> inspector;
        uint32_t inspectorWidth = 0;
        uint32_t inspectorHeight = 0;
        PrismaUI::Web::ViewId result = 0;
        {
            std::lock_guard lock(mutex_);
            const auto viewportWidth = host_.InspectorViewportWidth();
            const auto viewportHeight = host_.InspectorViewportHeight();
            for (auto& [_, state] : inspectors_) {
                if (!state.visible || !state.view || state.gesture == InspectorState::Gesture::None) continue;
                const int dx = x - state.lastCursorX;
                const int dy = y - state.lastCursorY;
                if (state.gesture == InspectorState::Gesture::Move) {
                    state.x = std::clamp(state.x + dx, 0, static_cast<int>(viewportWidth - state.width));
                    state.y = std::clamp(state.y + dy, 0, static_cast<int>(viewportHeight - state.height));
                } else {
                    const int minimumWidth = static_cast<int>(std::min(kInspectorMinimumDimension, viewportWidth));
                    const int minimumHeight = static_cast<int>(std::min(kInspectorMinimumDimension, viewportHeight));
                    state.width = static_cast<uint32_t>(std::clamp(
                        static_cast<int>(state.width) + dx, minimumWidth, static_cast<int>(viewportWidth)));
                    state.height = static_cast<uint32_t>(std::clamp(
                        static_cast<int>(state.height) + dy, minimumHeight, static_cast<int>(viewportHeight)));
                    state.x = std::clamp(state.x, 0, static_cast<int>(viewportWidth - state.width));
                    state.y = std::clamp(state.y, 0, static_cast<int>(viewportHeight - state.height));
                    inspector = state.view;
                    inspectorWidth = state.width;
                    inspectorHeight = state.height;
                }
                state.lastCursorX = x;
                state.lastCursorY = y;
                localX = std::clamp(x - state.x, 0, static_cast<int>(state.width) - 1);
                localY = std::clamp(y - state.y, 0, static_cast<int>(state.height) - 1);
                host_.RequestInspectorFrame();
                result = state.id;
                break;
            }
        }
        if (inspector) inspector->Resize(inspectorWidth, inspectorHeight);
        return result;
    }

    PrismaUI::Web::ViewId UltralightInspector::GestureTargetAt(
        int x, int y, int& localX, int& localY) const {
        std::lock_guard lock(mutex_);
        for (const auto& [_, state] : inspectors_) {
            if (!state.visible || !state.view || state.gesture == InspectorState::Gesture::None) continue;
            localX = std::clamp(x - state.x, 0, static_cast<int>(state.width) - 1);
            localY = std::clamp(y - state.y, 0, static_cast<int>(state.height) - 1);
            return state.id;
        }
        return 0;
    }

    void UltralightInspector::EndGesture() {
        std::lock_guard lock(mutex_);
        for (auto& [_, state] : inspectors_) {
            if (state.gesture == InspectorState::Gesture::None) continue;
            state.gesture = InspectorState::Gesture::None;
            return;
        }
    }

    void UltralightInspector::BeginGesture(
        PrismaUI::Web::ViewId inspectorId, int localX, int localY, InspectorState::Gesture gesture) {
        std::lock_guard lock(mutex_);
        for (auto& [_, state] : inspectors_) {
            if (state.id != inspectorId || !state.visible || !state.view) continue;
            state.gesture = gesture;
            state.lastCursorX = state.x + localX;
            state.lastCursorY = state.y + localY;
            return;
        }
    }

    void UltralightInspector::MoveBy(
        PrismaUI::Web::ViewId inspectorId, int dx, int dy) {
        host_.DispatchInspectorTask([this, inspectorId, dx, dy] {
            std::lock_guard lock(mutex_);
            const auto viewportWidth = host_.InspectorViewportWidth();
            const auto viewportHeight = host_.InspectorViewportHeight();
            for (auto& [_, state] : inspectors_) {
                if (state.id != inspectorId || !state.view) continue;
                state.x = std::clamp(state.x + dx, 0, static_cast<int>(viewportWidth - state.width));
                state.y = std::clamp(state.y + dy, 0, static_cast<int>(viewportHeight - state.height));
                host_.RequestInspectorFrame();
                return;
            }
        });
    }

    void UltralightInspector::ResizeBy(
        PrismaUI::Web::ViewId inspectorId, int dx, int dy) {
        host_.DispatchInspectorTask([this, inspectorId, dx, dy] {
            ultralight::RefPtr<ultralight::View> inspector;
            uint32_t inspectorWidth = 0;
            uint32_t inspectorHeight = 0;
            {
                std::lock_guard lock(mutex_);
                const auto viewportWidth = host_.InspectorViewportWidth();
                const auto viewportHeight = host_.InspectorViewportHeight();
                for (auto& [_, state] : inspectors_) {
                    if (state.id != inspectorId || !state.view) continue;
                    const int minimumWidth = static_cast<int>(std::min(kInspectorMinimumDimension, viewportWidth));
                    const int minimumHeight = static_cast<int>(std::min(kInspectorMinimumDimension, viewportHeight));
                    state.width = static_cast<uint32_t>(std::clamp(
                        static_cast<int>(state.width) + dx, minimumWidth, static_cast<int>(viewportWidth)));
                    state.height = static_cast<uint32_t>(std::clamp(
                        static_cast<int>(state.height) + dy, minimumHeight, static_cast<int>(viewportHeight)));
                    state.x = std::clamp(state.x, 0, static_cast<int>(viewportWidth - state.width));
                    state.y = std::clamp(state.y, 0, static_cast<int>(viewportHeight - state.height));
                    inspector = state.view;
                    inspectorWidth = state.width;
                    inspectorHeight = state.height;
                    break;
                }
            }
            if (inspector) {
                inspector->Resize(inspectorWidth, inspectorHeight);
                host_.RequestInspectorFrame();
            }
        });
    }

    bool UltralightInspector::GetFrame(uint64_t generation, PrismaUI::Web::InspectorFrame& out) const {
        std::lock_guard lock(mutex_);
        for (const auto& [_, state] : inspectors_) {
            if (!state.visible || !state.view) continue;
            PrismaUI::Web::RenderTargetSnapshot target;
            if (!host_.CopyPublishedInspectorTarget(state.id, generation, target)) continue;
            out.view = state.id;
            out.x = state.x;
            out.y = state.y;
            out.width = state.width;
            out.height = state.height;
            out.target = std::move(target);
            return out.target.validForComposition(generation);
        }
        return false;
    }

    PrismaUI::Web::ViewId UltralightInspector::TargetAt(
        int x, int y, int& localX, int& localY) const {
        std::lock_guard lock(mutex_);
        for (const auto& [_, state] : inspectors_) {
            if (!state.visible || !state.view || x < state.x || y < state.y ||
                x >= state.x + static_cast<int>(state.width) ||
                y >= state.y + static_cast<int>(state.height)) continue;
            localX = x - state.x;
            localY = y - state.y;
            return state.id;
        }
        return 0;
    }

    PrismaUI::Web::ViewId UltralightInspector::VisibleView() const {
        std::lock_guard lock(mutex_);
        for (const auto& [_, state] : inspectors_)
            if (state.visible && state.view) return state.id;
        return 0;
    }

    void UltralightInspector::InstallHost(ultralight::View& view, PrismaUI::Web::ViewId inspectorId) {
        auto context = view.LockJSContext();
        if (!context) return;
        ultralight::SetJSContext(context->ctx());
        auto global = ultralight::JSGlobalObject();
        global[ultralight::JSString("PrismaUI_MoveInspectorBy")] = ultralight::JSCallback(
            [this, inspectorId](const ultralight::JSObject&, const ultralight::JSArgs& args) {
                if (args.size() < 2) return;
                MoveBy(inspectorId, static_cast<int>(args[0].ToInteger()),
                       static_cast<int>(args[1].ToInteger()));
            });
        global[ultralight::JSString("PrismaUI_ResizeInspectorBy")] = ultralight::JSCallback(
            [this, inspectorId](const ultralight::JSObject&, const ultralight::JSArgs& args) {
                if (args.size() < 2) return;
                ResizeBy(inspectorId, static_cast<int>(args[0].ToInteger()),
                         static_cast<int>(args[1].ToInteger()));
            });
        global[ultralight::JSString("PrismaUI_BeginInspectorMove")] = ultralight::JSCallback(
            [this, inspectorId](const ultralight::JSObject&, const ultralight::JSArgs& args) {
                if (args.size() < 2) return;
                BeginMove(inspectorId, static_cast<int>(args[0].ToInteger()),
                          static_cast<int>(args[1].ToInteger()));
            });
        global[ultralight::JSString("PrismaUI_BeginInspectorResize")] = ultralight::JSCallback(
            [this, inspectorId](const ultralight::JSObject&, const ultralight::JSArgs& args) {
                if (args.size() < 2) return;
                BeginResize(inspectorId, static_cast<int>(args[0].ToInteger()),
                            static_cast<int>(args[1].ToInteger()));
            });
    }

}
