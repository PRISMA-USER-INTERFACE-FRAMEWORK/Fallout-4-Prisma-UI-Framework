#include "UltralightBackend.h"

#include <algorithm>
#include <utility>

namespace PrismaUI::WebRuntimeUltralight {

    ultralight::RefPtr<ultralight::View> UltralightBackend::OnCreateInspectorView(
        PrismaUI::Web::ViewId owner, bool isLocal, const ultralight::String&) {
        if (!isLocal) return {};
        const auto ownerRecord = viewManager_.Find(owner);
        if (!ownerRecord || ownerRecord->isInspector || ownerRecord->inspectorId || !renderer_) return {};

        ultralight::ViewConfig config;
        config.is_accelerated = accelerated_;
        config.is_transparent = false;
        config.initial_focus = false;
        auto inspector = renderer_->CreateView(std::min(width_, 900u), std::min(height_, 650u), config, session_);
        if (!inspector) return {};

        const PrismaUI::Web::ViewId inspectorId = NextViewId();
        auto record = std::make_unique<ViewRecord>();
        record->id = inspectorId;
        record->isInspector = true;
        record->inspectedView = owner;
        record->source.relativePath = "inspector/Main.html";
        record->owner = ownerRecord->owner;
        record->view = inspector;
        record->viewportSized = false;
        record->callbacks = std::make_unique<UltralightViewCallbacks>(*this, inspectorId);
        inspector->set_view_listener(record->callbacks.get());
        inspector->set_load_listener(record->callbacks.get());
        viewManager_.Add(std::move(record));
        ownerRecord->inspectorId = inspectorId;

        const uint32_t inspectorWidth = std::min(width_, 900u);
        const uint32_t inspectorHeight = std::min(height_, 650u);
        inspector_.Add(owner, inspectorId, inspector, inspectorWidth, inspectorHeight,
                       static_cast<int>(width_ - inspectorWidth), 0);
        InstallInspectorHost(*inspector, inspectorId);
        return inspector;
    }

    void UltralightBackend::OnInspectorRequestClose(PrismaUI::Web::ViewId inspectorId) {
        inspector_.OnInspectorRequestClose(inspectorId);
    }

    void UltralightBackend::NotifyInspectorState(PrismaUI::Web::ViewId owner, bool visible) {
        PrismaUI::Web::InspectorStateCallback callback;
        {
            std::lock_guard lock(stateMutex_);
            callback = inspectorStateCallback_;
        }
        if (callback) callback(owner, visible);
    }

    PrismaUI::Web::ViewId UltralightBackend::NextViewId() {
        return viewManager_.NextId();
    }

    bool UltralightBackend::BeginViewCreation(PrismaUI::Web::ViewId id) {
        return viewManager_.BeginCreation(id);
    }

    bool UltralightBackend::IsViewDestroyRequested(PrismaUI::Web::ViewId id) {
        return viewManager_.IsDestroyRequested(id);
    }

    void UltralightBackend::FailViewCreation(PrismaUI::Web::ViewId id,
                                             PrismaUI::Web::DomReadyCallback onReady,
                                             std::string detail) {
        viewManager_.FailCreation(
            id, std::move(onReady), std::move(detail),
            [this](PrismaUI::Web::ViewId view, PrismaUI::Web::ViewHealth health,
                   std::string message, std::string detail) {
                ReportError(view, health, std::move(message), std::move(detail));
            });
    }

    bool UltralightBackend::PublishCreatedView(PrismaUI::Web::ViewId id) {
        return viewManager_.PublishCreated(id);
    }

    void UltralightBackend::RemoveOwnerView(PrismaUI::Web::ViewId id) {
        CancelNativeGamepad(id, true);
        const auto record = viewManager_.Find(id);
        if (!record) return;
        const PrismaUI::Web::ViewId inspectorId = record->inspectorId;
        if (inspectorId) {
            const auto inspector = viewManager_.Find(inspectorId);
            if (inspector) {
                if (inspector->view) {
                    inspector->view->set_view_listener(nullptr);
                    inspector->view->set_load_listener(nullptr);
                    inspector->view = nullptr;
                }
                viewManager_.EraseRecord(inspectorId);
            }
            {
                std::lock_guard lock(stateMutex_);
                published_.erase(inspectorId);
            }
            inspector_.RemoveForOwner(id);
        }
        if (record->view) {
            record->view->set_view_listener(nullptr);
            record->view->set_load_listener(nullptr);
            record->view = nullptr;
        }
        viewManager_.EraseRecord(id);
    }

    void UltralightBackend::ErasePublishedView(PrismaUI::Web::ViewId id) {
        viewManager_.ErasePublished(id);
        std::lock_guard lock(stateMutex_);
        published_.erase(id);
    }

    void UltralightBackend::SetHealth(ViewRecord& record, PrismaUI::Web::ViewHealth health) {
        viewManager_.SetHealth(record, health);
    }

}
