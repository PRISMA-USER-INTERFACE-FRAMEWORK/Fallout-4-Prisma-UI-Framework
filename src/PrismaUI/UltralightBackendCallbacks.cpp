#include "UltralightBackend.h"

#include "ControllerActions.h"
#include "ViewOrigin.h"

#include <utility>

namespace PrismaUI::WebRuntimeUltralight {

    void UltralightBackend::OnConsole(PrismaUI::Web::ViewId id,
                                      const ultralight::ConsoleMessage& message) {
        documentCallbacks_.OnConsole(id, message);
    }

    void UltralightBackend::OnChangeURL(PrismaUI::Web::ViewId id, std::string url) {
        documentCallbacks_.OnChangeURL(id, std::move(url));
    }

    void UltralightBackend::OnBeginLoading(PrismaUI::Web::ViewId id, bool mainFrame, std::string url) {
        documentCallbacks_.OnBeginLoading(id, mainFrame, std::move(url));
    }

    void UltralightBackend::OnFinishLoading(PrismaUI::Web::ViewId id, bool mainFrame, std::string url) {
        documentCallbacks_.OnFinishLoading(id, mainFrame, std::move(url));
    }

    void UltralightBackend::OnFailLoading(PrismaUI::Web::ViewId id, bool mainFrame, std::string url,
                                          std::string description, std::string domain, int code) {
        documentCallbacks_.OnFailLoading(id, mainFrame, std::move(url), std::move(description),
                                         std::move(domain), code);
    }

    void UltralightBackend::OnWindowObjectReady(PrismaUI::Web::ViewId id, bool mainFrame, std::string url) {
        documentCallbacks_.OnWindowObjectReady(id, mainFrame, std::move(url));
    }

    void UltralightBackend::OnDomReady(PrismaUI::Web::ViewId id, bool mainFrame, std::string url) {
        documentCallbacks_.OnDomReady(id, mainFrame, std::move(url));
    }

    void UltralightBackend::OnDocumentBeginLoading(PrismaUI::Web::ViewId id) {
        if (controllerKeyView_ == id) {
            controllerKeys_ = {};
            controllerKeyView_ = 0;
        }
        CancelNativeGamepad(id);
        ControllerActions::InvalidateBridge(id);
    }

    void UltralightBackend::OnTrustedDocumentReady(PrismaUI::Web::ViewId id) {
        if (ControllerActions::HasMappings(id)) {
            ControllerActions::InstallBridge(id);
        }
    }

    void UltralightBackend::ReleaseNativeGamepadForInspector() {
        CancelNativeGamepad(0, true);
    }

    void UltralightBackend::FocusOwnerView(PrismaUI::Web::ViewId owner) {
        FocusView(owner);
    }

    ViewRecord* UltralightBackend::FindDocumentView(PrismaUI::Web::ViewId id) noexcept {
        return viewManager_.Find(id);
    }

    void UltralightBackend::SetDocumentHealth(ViewRecord& record,
                                              PrismaUI::Web::ViewHealth health) {
        viewManager_.SetHealth(record, health);
    }

    std::string UltralightBackend::DocumentFileUrl(std::string_view relativePath) const {
        return "file:///" + PrismaUI::ViewOrigin::EncodeUrlPath(relativePath);
    }

    void UltralightBackend::ReportDocumentError(PrismaUI::Web::ViewId id,
                                                PrismaUI::Web::ViewHealth health,
                                                std::string message, std::string detail) {
        ReportError(id, health, std::move(message), std::move(detail));
    }

    bool UltralightBackend::DispatchDocument(std::function<void()> task) {
        return DispatchDeferred(std::move(task));
    }

    ViewRecord* UltralightBackend::FindInspectorRecord(PrismaUI::Web::ViewId id) noexcept {
        return viewManager_.Find(id);
    }

    uint32_t UltralightBackend::InspectorViewportWidth() const noexcept {
        std::lock_guard lock(stateMutex_);
        return width_;
    }

    uint32_t UltralightBackend::InspectorViewportHeight() const noexcept {
        std::lock_guard lock(stateMutex_);
        return height_;
    }

    void UltralightBackend::RequestInspectorFrame() noexcept {
        dispatchQueue_.RequestFrame();
    }

    void UltralightBackend::DispatchInspector(PrismaUI::Web::ViewId id,
                                              std::function<void()> task) {
        DispatchForView(id, std::move(task));
    }

    void UltralightBackend::DispatchInspectorTask(std::function<void()> task) {
        Dispatch(std::move(task));
    }

    bool UltralightBackend::CopyPublishedInspectorTarget(
        PrismaUI::Web::ViewId id, uint64_t generation,
        PrismaUI::Web::RenderTargetSnapshot& target) const {
        std::lock_guard lock(stateMutex_);
        const auto it = published_.find(id);
        if (it == published_.end() || it->second.generation != generation || !it->second.srv) return false;
        const auto& published = it->second;
        target.srv = published.srv;
        target.viewportWidth = published.width;
        target.viewportHeight = published.height;
        target.textureWidth = published.textureWidth;
        target.textureHeight = published.textureHeight;
        target.format = published.format;
        target.uvLeft = published.uvLeft;
        target.uvTop = published.uvTop;
        target.uvRight = published.uvRight;
        target.uvBottom = published.uvBottom;
        target.publishGeneration = published.generation;
        target.contentGeneration = published.contentGeneration;
        target.deviceEpoch = published.deviceEpoch;
        return target.validForComposition(generation);
    }

    void UltralightBackend::InstallInspectorHost(ultralight::View& view,
                                                 PrismaUI::Web::ViewId inspectorId) {
        inspector_.InstallHost(view, inspectorId);
    }

    void UltralightBackend::ReportError(PrismaUI::Web::ViewId id,
                                        PrismaUI::Web::ViewHealth health,
                                        std::string message, std::string detail) {
        PrismaUI::Web::ViewErrorCallback callback;
        {
            std::lock_guard lock(stateMutex_);
            callback = errorCallback_;
        }
        if (callback) callback(id, health, std::move(message), std::move(detail));
    }

}
