#pragma once

#include "IWebBackend.h"

#pragma warning(push)
#pragma warning(disable : 4100)
#include <Ultralight/Ultralight.h>
#pragma warning(pop)

#include <string>

namespace PrismaUI::WebRuntimeUltralight {

    class IUltralightViewCallbackSink {
    public:
        virtual ~IUltralightViewCallbackSink() = default;

        virtual void OnConsole(PrismaUI::Web::ViewId id, const ultralight::ConsoleMessage& message) = 0;
        virtual void OnChangeURL(PrismaUI::Web::ViewId id, std::string url) = 0;
        virtual void OnBeginLoading(PrismaUI::Web::ViewId id, bool mainFrame, std::string url) = 0;
        virtual void OnFinishLoading(PrismaUI::Web::ViewId id, bool mainFrame, std::string url) = 0;
        virtual void OnFailLoading(PrismaUI::Web::ViewId id, bool mainFrame, std::string url,
                                   std::string description, std::string domain, int code) = 0;
        virtual void OnWindowObjectReady(PrismaUI::Web::ViewId id, bool mainFrame, std::string url) = 0;
        virtual void OnDomReady(PrismaUI::Web::ViewId id, bool mainFrame, std::string url) = 0;
        virtual ultralight::RefPtr<ultralight::View> OnCreateInspectorView(
            PrismaUI::Web::ViewId id, bool isLocal, const ultralight::String& inspectedUrl) = 0;
        virtual void OnInspectorRequestClose(PrismaUI::Web::ViewId id) = 0;
    };

    class UltralightViewCallbacks final : public ultralight::ViewListener,
                                          public ultralight::LoadListener {
    public:
        UltralightViewCallbacks(IUltralightViewCallbackSink& sink, PrismaUI::Web::ViewId id);

        void OnAddConsoleMessage(ultralight::View*, const ultralight::ConsoleMessage&) override;
        void OnChangeURL(ultralight::View*, const ultralight::String&) override;
        void OnBeginLoading(ultralight::View*, uint64_t, bool, const ultralight::String&) override;
        void OnFinishLoading(ultralight::View*, uint64_t, bool, const ultralight::String&) override;
        void OnFailLoading(ultralight::View*, uint64_t, bool, const ultralight::String&,
                           const ultralight::String&, const ultralight::String&, int) override;
        void OnWindowObjectReady(ultralight::View*, uint64_t, bool, const ultralight::String&) override;
        void OnDOMReady(ultralight::View*, uint64_t, bool, const ultralight::String&) override;
        ultralight::RefPtr<ultralight::View> OnCreateInspectorView(
            ultralight::View*, bool, const ultralight::String&) override;
        void OnRequestClose(ultralight::View*) override;

    private:
        IUltralightViewCallbackSink& sink_;
        PrismaUI::Web::ViewId id_;
    };

}
