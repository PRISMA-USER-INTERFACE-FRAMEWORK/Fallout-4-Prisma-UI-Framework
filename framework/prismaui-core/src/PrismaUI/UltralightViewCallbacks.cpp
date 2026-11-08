#include "UltralightViewCallbacks.h"

#include "UltralightString.h"

namespace PrismaUI::WebRuntimeUltralight {

    UltralightViewCallbacks::UltralightViewCallbacks(IUltralightViewCallbackSink& sink,
                                                     PrismaUI::Web::ViewId id) :
        sink_(sink), id_(id) {}

    void UltralightViewCallbacks::OnAddConsoleMessage(
        ultralight::View*, const ultralight::ConsoleMessage& message) {
        sink_.OnConsole(id_, message);
    }

    void UltralightViewCallbacks::OnChangeURL(ultralight::View*, const ultralight::String& url) {
        sink_.OnChangeURL(id_, ToUtf8(url));
    }

    void UltralightViewCallbacks::OnBeginLoading(
        ultralight::View*, uint64_t, bool mainFrame, const ultralight::String& url) {
        sink_.OnBeginLoading(id_, mainFrame, ToUtf8(url));
    }

    void UltralightViewCallbacks::OnFinishLoading(
        ultralight::View*, uint64_t, bool mainFrame, const ultralight::String& url) {
        sink_.OnFinishLoading(id_, mainFrame, ToUtf8(url));
    }

    void UltralightViewCallbacks::OnFailLoading(
        ultralight::View*, uint64_t, bool mainFrame, const ultralight::String& url,
        const ultralight::String& description, const ultralight::String& domain, int code) {
        sink_.OnFailLoading(id_, mainFrame, ToUtf8(url), ToUtf8(description), ToUtf8(domain), code);
    }

    void UltralightViewCallbacks::OnWindowObjectReady(
        ultralight::View*, uint64_t, bool mainFrame, const ultralight::String& url) {
        sink_.OnWindowObjectReady(id_, mainFrame, ToUtf8(url));
    }

    void UltralightViewCallbacks::OnDOMReady(
        ultralight::View*, uint64_t, bool mainFrame, const ultralight::String& url) {
        sink_.OnDomReady(id_, mainFrame, ToUtf8(url));
    }

    ultralight::RefPtr<ultralight::View> UltralightViewCallbacks::OnCreateInspectorView(
        ultralight::View*, bool isLocal, const ultralight::String& inspectedUrl) {
        return sink_.OnCreateInspectorView(id_, isLocal, inspectedUrl);
    }

    void UltralightViewCallbacks::OnRequestClose(ultralight::View*) {
        sink_.OnInspectorRequestClose(id_);
    }

}
