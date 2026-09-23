#include "UltralightBackend.h"

#include "FreezeDiagnostics.h"
#include "PrismaFileSystem.h"
#include "ViewOrigin.h"

#include <utility>

namespace PrismaUI::WebRuntimeUltralight {

    PrismaUI::Web::ViewId UltralightBackend::CreateView(
        const PrismaUI::Web::ViewSource& source, const std::string& owner,
        PrismaUI::Web::DomReadyCallback onReady) {
        const std::string normalized = PrismaUI::ViewOrigin::NormalizeRelativePath(source.relativePath);
        if (normalized.empty() || normalized != source.relativePath ||
            PrismaUI::ViewOrigin::ScopeForPath(normalized).empty())
            return 0;
        const PrismaUI::Web::ViewId id = viewManager_.Reserve(normalized, owner, [this] {
            std::lock_guard lock(stateMutex_);
            return ready_ && !dispatchQueue_.IsStopping();
        });
        if (!id) return 0;
        const bool admitted = DispatchDeferred(
            [this, id, normalized, owner, onReady = std::move(onReady)]() mutable {
                FreezeDiagnostics::ScopedStage createStage(
                    FreezeDiagnostics::Lane::UltralightOwner, FreezeDiagnostics::Stage::CreateView);
                if (!BeginViewCreation(id)) return;
                if (!renderer_ || dispatchQueue_.IsStopping() || !fileSystem_) {
                    FailViewCreation(id, std::move(onReady), "backend is stopping or unavailable");
                    return;
                }
                if (IsViewDestroyRequested(id)) return;
                auto buffer = fileSystem_->OpenFile(ultralight::String(normalized.c_str()));
                if (!buffer || !buffer->data()) {
                    FailViewCreation(id, std::move(onReady), "view source could not be opened");
                    return;
                }
                if (IsViewDestroyRequested(id)) return;
                auto record = std::make_unique<ViewRecord>();
                record->id = id;
                record->source.relativePath = normalized;
                record->owner = owner;
                record->domReady = std::move(onReady);
                record->width = width_;
                record->height = height_;
                ultralight::ViewConfig config;
                config.is_accelerated = accelerated_;
                config.is_transparent = true;
                config.initial_focus = false;
                if (IsViewDestroyRequested(id)) return;
                record->view = renderer_->CreateView(width_, height_, config, session_);
                if (!record->view) {
                    FailViewCreation(id, std::move(record->domReady),
                                     "Ultralight renderer could not create the view");
                    return;
                }
                if (IsViewDestroyRequested(id)) {
                    record->view->set_view_listener(nullptr);
                    record->view->set_load_listener(nullptr);
                    return;
                }
                record->callbacks = std::make_unique<UltralightViewCallbacks>(*this, id);
                record->view->set_view_listener(record->callbacks.get());
                record->view->set_load_listener(record->callbacks.get());
                const std::string html(static_cast<const char*>(buffer->data()), buffer->size());
                viewManager_.Add(std::move(record));
                auto* stored = viewManager_.Find(id);
                if (!stored) return;
                if (!PublishCreatedView(id)) {
                    RemoveOwnerView(id);
                    return;
                }
                if (IsViewDestroyRequested(id)) {
                    RemoveOwnerView(id);
                    return;
                }
                const std::string fileUrl = DocumentFileUrl(normalized);
                stored->view->LoadHTML(ultralight::String(html.c_str()), ultralight::String(fileUrl.c_str()));
            });
        if (!admitted) {
            ErasePublishedView(id);
            return 0;
        }
        return id;
    }

    void UltralightBackend::DestroyView(PrismaUI::Web::ViewId id) {
        if (!id) return;
        CancelNativeGamepad(id, true);
        if (!viewManager_.RequestDestroy(id)) return;
        if (!DispatchDeferred([this, id] {
                FreezeDiagnostics::ScopedStage destroyStage(
                    FreezeDiagnostics::Lane::UltralightOwner, FreezeDiagnostics::Stage::DestroyView, id);
                RemoveOwnerView(id);
                ErasePublishedView(id);
            })) {
            ErasePublishedView(id);
        }
    }

    bool UltralightBackend::IsViewValid(PrismaUI::Web::ViewId id) {
        return viewManager_.IsValid(id);
    }

    PrismaUI::Web::ViewHealth UltralightBackend::GetViewHealth(PrismaUI::Web::ViewId id) {
        return viewManager_.GetHealth(id);
    }

    void UltralightBackend::EnumerateViews(const PrismaUI::Web::ViewEnumCallback& callback) {
        viewManager_.Enumerate(callback);
    }

    void UltralightBackend::FocusView(PrismaUI::Web::ViewId id) {
        {
            std::lock_guard lock(gamepadMutex_);
            gamepadFocusedView_ = id;
        }
        if (!UsesNativeGamepad(id)) CancelNativeGamepad();
        DispatchForView(id, [this, id] {
            if (const auto record = viewManager_.Find(id); record && record->view) record->view->Focus();
        });
    }

    void UltralightBackend::UnfocusView(PrismaUI::Web::ViewId id) {
        CancelNativeGamepad(id, true);
        DispatchForView(id, [this, id] {
            if (const auto record = viewManager_.Find(id); record && record->view) record->view->Unfocus();
        });
    }

    void UltralightBackend::CreateInspectorView(PrismaUI::Web::ViewId id) {
        DispatchForView(id, [this, id] {
            const auto record = viewManager_.Find(id);
            if (!record || record->isInspector || !record->view || record->inspectorId) return;
            record->view->CreateLocalInspectorView();
        });
    }

    void UltralightBackend::SetInspectorStateCallback(PrismaUI::Web::InspectorStateCallback callback) {
        std::lock_guard lock(stateMutex_);
        inspectorStateCallback_ = std::move(callback);
    }

    void UltralightBackend::SetInspectorVisibility(PrismaUI::Web::ViewId id, bool visible) {
        inspector_.SetVisibility(id, visible);
    }

    bool UltralightBackend::IsInspectorVisible(PrismaUI::Web::ViewId id) {
        return inspector_.IsVisible(id);
    }

    void UltralightBackend::SetInspectorBounds(PrismaUI::Web::ViewId id, float x, float y,
                                               uint32_t width, uint32_t height) {
        inspector_.SetBounds(id, x, y, width, height);
    }

    void UltralightBackend::BeginInspectorMove(PrismaUI::Web::ViewId inspectorId, int localX, int localY) {
        inspector_.BeginMove(inspectorId, localX, localY);
    }

    void UltralightBackend::BeginInspectorResize(PrismaUI::Web::ViewId inspectorId, int localX, int localY) {
        inspector_.BeginResize(inspectorId, localX, localY);
    }

    PrismaUI::Web::ViewId UltralightBackend::UpdateInspectorGesture(int x, int y, int& localX, int& localY) {
        return inspector_.UpdateGesture(x, y, localX, localY);
    }

    PrismaUI::Web::ViewId UltralightBackend::InspectorGestureTargetAt(
        int x, int y, int& localX, int& localY) {
        return inspector_.GestureTargetAt(x, y, localX, localY);
    }

    void UltralightBackend::EndInspectorGesture() {
        inspector_.EndGesture();
    }

    bool UltralightBackend::GetInspectorFrame(uint64_t generation, PrismaUI::Web::InspectorFrame& out) {
        return inspector_.GetFrame(generation, out);
    }

    PrismaUI::Web::ViewId UltralightBackend::InspectorTargetAt(
        int x, int y, int& localX, int& localY) {
        return inspector_.TargetAt(x, y, localX, localY);
    }

    PrismaUI::Web::ViewId UltralightBackend::VisibleInspectorView() {
        return inspector_.VisibleView();
    }

}
