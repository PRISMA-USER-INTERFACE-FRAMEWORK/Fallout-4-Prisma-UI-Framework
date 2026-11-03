#pragma once

#include "UltralightViewManager.h"

#pragma warning(push)
#pragma warning(disable : 4100)
#include <Ultralight/Ultralight.h>
#pragma warning(pop)

#include <functional>
#include <string>
#include <string_view>

namespace PrismaUI::WebRuntimeUltralight {

    class IUltralightDocumentCallbackHost {
    public:
        virtual ~IUltralightDocumentCallbackHost() = default;

        virtual ViewRecord* FindDocumentView(PrismaUI::Web::ViewId id) noexcept = 0;
        virtual void SetDocumentHealth(ViewRecord& record, PrismaUI::Web::ViewHealth health) = 0;
        virtual std::string DocumentFileUrl(std::string_view relativePath) const = 0;
        virtual void ReportDocumentError(PrismaUI::Web::ViewId id, PrismaUI::Web::ViewHealth health,
                                          std::string message, std::string detail) = 0;
        virtual bool DispatchDocument(std::function<void()> task) = 0;
        virtual void InstallInspectorHost(ultralight::View& view, PrismaUI::Web::ViewId inspectorId) = 0;
        virtual void OnDocumentBeginLoading(PrismaUI::Web::ViewId id) = 0;
        virtual void OnTrustedDocumentReady(PrismaUI::Web::ViewId id) = 0;
    };

    class UltralightDocumentCallbacks final {
    public:
        explicit UltralightDocumentCallbacks(IUltralightDocumentCallbackHost& host);

        void OnConsole(PrismaUI::Web::ViewId id, const ultralight::ConsoleMessage& message);
        void OnChangeURL(PrismaUI::Web::ViewId id, std::string url);
        void OnBeginLoading(PrismaUI::Web::ViewId id, bool mainFrame, std::string url);
        void OnFinishLoading(PrismaUI::Web::ViewId id, bool mainFrame, std::string url);
        void OnFailLoading(PrismaUI::Web::ViewId id, bool mainFrame, std::string url,
                           std::string description, std::string domain, int code);
        void OnWindowObjectReady(PrismaUI::Web::ViewId id, bool mainFrame, std::string url);
        void OnDomReady(PrismaUI::Web::ViewId id, bool mainFrame, std::string url);
        void InvokeFunction(ViewRecord& record, const std::string& name, const std::string& argumentJson);
        void InstallListener(ViewRecord& record, const std::string& name);

    private:
        [[nodiscard]] bool IsTrustedDocument(const ViewRecord& record, std::string_view url) const;
        void BlockNavigation(ViewRecord& record, std::string url);
        void InstallListeners(ViewRecord& record);
        void FlushPendingCalls(PrismaUI::Web::ViewId id);
        void ReportError(PrismaUI::Web::ViewId id, PrismaUI::Web::ViewHealth health,
                         std::string message, std::string detail);

        IUltralightDocumentCallbackHost& host_;
    };

}
