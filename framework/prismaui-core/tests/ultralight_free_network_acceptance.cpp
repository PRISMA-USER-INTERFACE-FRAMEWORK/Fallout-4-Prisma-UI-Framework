#include <AppCore/Platform.h>
#include <Ultralight/Ultralight.h>

#include "PrismaUI/NetworkSandbox.h"
#include "PrismaUI/WebNetworkPolicy.h"

#include <WinSock2.h>
#include <Windows.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

namespace {

    class HttpProbe {
    public:
        explicit HttpProbe(std::string response) : response_(std::move(response)) {}

        bool Start() {
            WSADATA data{};
            if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return false;
            winsockStarted_ = true;
            listener_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (listener_ == INVALID_SOCKET) return false;

            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            address.sin_port = 0;
            if (::bind(listener_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR ||
                ::listen(listener_, 8) == SOCKET_ERROR) {
                closesocket(listener_);
                listener_ = INVALID_SOCKET;
                return false;
            }
            int length = sizeof(address);
            if (::getsockname(listener_, reinterpret_cast<sockaddr*>(&address), &length) == SOCKET_ERROR) {
                closesocket(listener_);
                listener_ = INVALID_SOCKET;
                return false;
            }
            port_ = ntohs(address.sin_port);
            worker_ = std::thread([this] { Run(); });
            return true;
        }

        void Stop() {
            if (!winsockStarted_) return;
            stopping_.store(true);
            if (listener_ != INVALID_SOCKET) {
                closesocket(listener_);
                listener_ = INVALID_SOCKET;
            }
            if (worker_.joinable()) worker_.join();
            WSACleanup();
            winsockStarted_ = false;
        }

        ~HttpProbe() { Stop(); }

        [[nodiscard]] uint16_t port() const { return port_; }
        [[nodiscard]] unsigned requests() const { return requests_.load(); }

    private:
        void Run() {
            while (!stopping_.load()) {
                fd_set readable;
                FD_ZERO(&readable);
                FD_SET(listener_, &readable);
                timeval timeout{0, 100000};
                if (::select(0, &readable, nullptr, nullptr, &timeout) <= 0) continue;
                SOCKET client = ::accept(listener_, nullptr, nullptr);
                if (client == INVALID_SOCKET) continue;
                requests_.fetch_add(1);
                ::send(client, response_.data(), static_cast<int>(response_.size()), 0);
                closesocket(client);
            }
        }

        SOCKET listener_ = INVALID_SOCKET;
        std::thread worker_;
        std::atomic_bool stopping_{false};
        std::atomic_uint requests_{0};
        std::string response_;
        uint16_t port_ = 0;
        bool winsockStarted_ = false;
    };

    class AcceptanceFileSystem final : public ultralight::FileSystem {
    public:
        AcceptanceFileSystem(std::string acceptanceHtml, std::string redirectHtml, std::string navigationHtml)
            : acceptanceHtml_(std::move(acceptanceHtml)), redirectHtml_(std::move(redirectHtml)),
              navigationHtml_(std::move(navigationHtml)),
              delegate_(ultralight::GetPlatformFileSystem(".")) {}

        bool FileExists(const ultralight::String& path) override {
            const auto value = ToUtf8(path);
            if (value.find("acceptance.html") != std::string::npos ||
                value.find("redirect.html") != std::string::npos ||
                value.find("navigation.html") != std::string::npos)
                return true;
            return delegate_->FileExists(path);
        }

        ultralight::String GetFileMimeType(const ultralight::String& path) override {
            if (FileExists(path) && (ToUtf8(path).find("acceptance.html") != std::string::npos ||
                                     ToUtf8(path).find("redirect.html") != std::string::npos ||
                                     ToUtf8(path).find("navigation.html") != std::string::npos))
                return ultralight::String("text/html");
            return delegate_->GetFileMimeType(path);
        }

        ultralight::String GetFileCharset(const ultralight::String& path) override {
            if (FileExists(path) && (ToUtf8(path).find("acceptance.html") != std::string::npos ||
                                     ToUtf8(path).find("redirect.html") != std::string::npos ||
                                     ToUtf8(path).find("navigation.html") != std::string::npos))
                return ultralight::String("UTF-8");
            return delegate_->GetFileCharset(path);
        }

        ultralight::RefPtr<ultralight::Buffer> OpenFile(const ultralight::String& path) override {
            const auto value = ToUtf8(path);
            const std::string* html = nullptr;
            if (value.find("acceptance.html") != std::string::npos) html = &acceptanceHtml_;
            if (value.find("redirect.html") != std::string::npos) html = &redirectHtml_;
            if (value.find("navigation.html") != std::string::npos) html = &navigationHtml_;
            if (!html) return delegate_->OpenFile(path);
            return ultralight::Buffer::CreateFromCopy(html->data(), html->size());
        }

    private:
        static std::string ToUtf8(const ultralight::String& value) {
            const auto& utf8 = value.utf8();
            return std::string(utf8.data(), utf8.size());
        }

        std::string acceptanceHtml_;
        std::string redirectHtml_;
        std::string navigationHtml_;
        ultralight::FileSystem* delegate_ = nullptr;
    };

    class AcceptanceListener final : public ultralight::ViewListener, public ultralight::LoadListener {
    public:
        void OnDOMReady(ultralight::View*, uint64_t, bool mainFrame, const ultralight::String& url) override {
            if (mainFrame) {
                domReady = true;
                trustedDocumentReady = enforceTrustedNavigation && IsTrustedDocument(ToUtf8(url));
            }
        }

        void OnFailLoading(ultralight::View*, uint64_t, bool mainFrame, const ultralight::String&,
                           const ultralight::String&, const ultralight::String&, int) override {
            if (mainFrame) loadFailed = true;
        }

        void OnChangeURL(ultralight::View* view, const ultralight::String& url) override {
            changedUrl = ToUtf8(url);
            if (!enforceTrustedNavigation || IsTrustedDocument(changedUrl)) return;
            remoteNavigationObserved = true;
            if (trustedDocumentReady) BlockNavigation(view, changedUrl);
        }

        void OnBeginLoading(ultralight::View* view, uint64_t, bool mainFrame, const ultralight::String& url) override {
            if (!mainFrame) return;
            beginUrl = ToUtf8(url);
            if (enforceTrustedNavigation && !IsTrustedDocument(beginUrl)) {
                remoteNavigationObserved = true;
                BlockNavigation(view, beginUrl);
            }
        }

        void ConfigureTrustedNavigation(std::string url) {
            enforceTrustedNavigation = true;
            trustedUrl = std::move(url);
        }

        bool IsTrustedDocument(const std::string& url) const {
            return PrismaUI::WebNetworkPolicy::IsTrustedLocalDocumentUrl(url, trustedUrl);
        }

        void BlockNavigation(ultralight::View* view, const std::string& url) {
            if (blockedNavigation) return;
            blockedNavigation = true;
            trustedDocumentReady = false;
            if (view) {
                const auto fallback = PrismaUI::NetworkSandbox::InjectContentSecurityPolicyIntoHtml("<body></body>");
                view->LoadHTML(fallback.c_str(), trustedUrl.c_str());
            }
        }

        static std::string ToUtf8(const ultralight::String& value) {
            const auto& utf8 = value.utf8();
            return std::string(utf8.data(), utf8.size());
        }

        bool domReady = false;
        bool loadFailed = false;
        bool enforceTrustedNavigation = false;
        bool trustedDocumentReady = false;
        bool blockedNavigation = false;
        bool remoteNavigationObserved = false;
        std::string beginUrl;
        std::string changedUrl;
        std::string trustedUrl;
    };

    std::string OkResponse() {
        return "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    }

    std::string RedirectResponse(const uint16_t forbiddenPort) {
        return "HTTP/1.1 302 Found\r\nLocation: http://127.0.0.1:" +
               std::to_string(forbiddenPort) + "/forbidden\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    }

    std::string BuildHtml(const uint16_t port) {
        const std::string endpoint = "http://127.0.0.1:" + std::to_string(port);
        const std::string wsEndpoint = "ws://127.0.0.1:" + std::to_string(port);
        const std::string source = R"html(<!doctype html><html><head>
<script>
window.__prisma_acceptance = {
  fetch: "pending", xhr: "pending", websocket: "pending", worker: "pending",
  script: "pending", style: "pending", image: "pending", font: "pending",
  media: "pending", frame: "pending", redirect: "pending", form: "pending"
};
window.__prisma_acceptance_signals = {};
function blocked(name, observed) {
  window.__prisma_acceptance[name] = "blocked";
  if (observed) window.__prisma_acceptance_signals[name] = true;
}
function loaded(name) { window.__prisma_acceptance[name] = "loaded"; }
document.addEventListener('securitypolicyviolation', function(event) {
  var uri = String(event.blockedURI || '');
  var directive = String(event.effectiveDirective || event.violatedDirective || '');
  if (uri.indexOf(')html" + endpoint + R"html(') === 0) {
    if (directive.indexOf('script') === 0 && uri.indexOf('/script.js') !== -1) blocked('script', true);
    if (directive.indexOf('style') === 0) blocked('style', true);
    if (directive.indexOf('font') === 0) blocked('font', true);
    if (directive.indexOf('img') === 0 && uri.indexOf('/redirect') !== -1) blocked('redirect', true);
    if (directive.indexOf('img') === 0 && uri.indexOf('/redirect') === -1) blocked('image', true);
    if (directive.indexOf('media') === 0) blocked('media', true);
    if (directive.indexOf('frame') === 0) blocked('frame', true);
    if (directive.indexOf('connect') === 0) {
      if (uri.indexOf('/fetch') !== -1) blocked('fetch', true);
      if (uri.indexOf('/xhr') !== -1) blocked('xhr', true);
      if (uri.indexOf('/socket') !== -1) blocked('websocket', true);
    }
    if (directive.indexOf('worker') === 0 || (directive.indexOf('script') === 0 && uri.indexOf('/worker.js') !== -1))
      blocked('worker', true);
    if (directive.indexOf('form') === 0) blocked('form', true);
  }
});
</script>
<script src=")html" + endpoint + R"html(/script.js" onload="loaded('script')" onerror="blocked('script', true)"></script>
<link rel="stylesheet" href=")html" + endpoint + R"html(/style.css" onload="loaded('style')" onerror="blocked('style', true)">
<link rel="preload" as="font" href=")html" + endpoint + R"html(/font.woff2" onload="loaded('font')" onerror="blocked('font', true)">
</head><body>
<img src=")html" + endpoint + R"html(/image.png" onload="loaded('image')" onerror="blocked('image', true)">
<video id="media" preload="auto" src=")html" + endpoint + R"html(/media.mp4" onloadeddata="loaded('media')" onerror="blocked('media', true)"></video>
<iframe src=")html" + endpoint + R"html(/frame.html" onload="loaded('frame')" onerror="blocked('frame', true)"></iframe>
<img src=")html" + endpoint + R"html(/redirect" onload="loaded('redirect')" onerror="blocked('redirect', true)">
<form id="form" action=")html" + endpoint + R"html(/form" method="get"></form>
<script>
fetch(")html" + endpoint + R"html(/fetch").then(function(){loaded('fetch');}, function(){blocked('fetch', true);});
try {
  var request = new XMLHttpRequest();
  request.open('GET', ")html" + endpoint + R"html(/xhr");
  request.onload = function(){loaded('xhr');};
  request.onerror = function(){blocked('xhr', true);};
  request.send();
} catch (e) { blocked('xhr', true); }
try {
  var socket = new WebSocket(")html" + wsEndpoint + R"html(/socket");
  socket.onopen = function(){loaded('websocket');};
  socket.onerror = function(){blocked('websocket', true);};
} catch (e) { blocked('websocket', true); }
try {
  var worker = new Worker(")html" + endpoint + R"html(/worker.js");
  worker.onmessage = function(){loaded('worker');};
  worker.onerror = function(){blocked('worker', true);};
  worker.postMessage('probe');
} catch (e) { blocked('worker', true); }
try { document.getElementById('form').submit(); } catch (e) { blocked('form', true); }
try { document.getElementById('media').load(); } catch (e) { blocked('media', true); }
setTimeout(function(){ window.__prisma_acceptance_done = true; }, 1500);
</script></body></html>)html";
        return PrismaUI::NetworkSandbox::InjectContentSecurityPolicyIntoHtml(source);
    }

    std::string BuildRedirectHtml(const uint16_t redirectPort) {
        const std::string endpoint = "http://127.0.0.1:" + std::to_string(redirectPort);
        return "<!doctype html><html><head><meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; script-src 'unsafe-inline'; img-src " +
               endpoint + ";\"></head><body><script>window.__prisma_redirect_done=false;</script><img src=\"" + endpoint +
               "/redirect\" onload=\"window.__prisma_redirect_done='loaded'\" onerror=\"window.__prisma_redirect_done='blocked'\"><script>setTimeout(function(){if(!window.__prisma_redirect_done) window.__prisma_redirect_done='timeout';},1500);</script></body></html>";
    }

    std::string BuildNavigationHtml(const uint16_t navigationPort) {
        const std::string endpoint = "http://127.0.0.1:" + std::to_string(navigationPort);
        const std::string source = "<!doctype html><html><body><script>setTimeout(function(){window.location.href='" +
                                   endpoint + "/navigation';},0);</script></body></html>";
        return PrismaUI::NetworkSandbox::InjectContentSecurityPolicyIntoHtml(source);
    }

    std::string ToUtf8(const ultralight::String& value) {
        const auto& utf8 = value.utf8();
        return std::string(utf8.data(), utf8.size());
    }

    bool Contains(const std::string& value, const char* needle) {
        return value.find(needle) != std::string::npos;
    }

    bool PumpUntil(ultralight::Renderer* renderer, const std::function<bool()>& predicate, const int ticks) {
        for (int tick = 0; tick != ticks; ++tick) {
            renderer->Update();
            renderer->RefreshDisplay(0);
            renderer->Render();
            if (predicate()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return predicate();
    }

}

int main() {
    if (!std::getenv("PRISMA_ULTRALIGHT_FREE_RUNTIME")) {
        std::cout << "ultralight free network acceptance skipped: PRISMA_ULTRALIGHT_FREE_RUNTIME is unset\n";
        return 0;
    }

    HttpProbe forbiddenProbe{OkResponse()};
    if (!forbiddenProbe.Start()) {
        std::cerr << "unable to start forbidden loopback acceptance server\n";
        return 1;
    }
    HttpProbe redirectProbe{RedirectResponse(forbiddenProbe.port())};
    if (!redirectProbe.Start()) {
        std::cerr << "unable to start redirect loopback acceptance server\n";
        return 1;
    }
    HttpProbe probe{OkResponse()};
    if (!probe.Start()) {
        std::cerr << "unable to start loopback acceptance server\n";
        return 1;
    }
    HttpProbe navigationProbe{OkResponse()};
    if (!navigationProbe.Start()) {
        std::cerr << "unable to start top-level navigation acceptance server\n";
        return 1;
    }

    AcceptanceFileSystem fileSystem(BuildHtml(probe.port()), BuildRedirectHtml(redirectProbe.port()),
                                    BuildNavigationHtml(navigationProbe.port()));
    ultralight::Config config;
    config.cache_path = "";
    auto& platform = ultralight::Platform::instance();
    platform.set_config(config);
    platform.set_file_system(&fileSystem);
    platform.set_font_loader(ultralight::GetPlatformFontLoader());
    platform.set_gpu_driver(nullptr);

    auto renderer = ultralight::Renderer::Create();
    if (!renderer) {
        std::cerr << "Ultralight Free Renderer::Create failed\n";
        return 1;
    }
    auto session = renderer->CreateSession(false, "free-network-acceptance");
    if (!session) {
        std::cerr << "Ultralight Free Session creation failed\n";
        return 1;
    }

    ultralight::ViewConfig viewConfig;
    viewConfig.is_accelerated = false;
    auto view = renderer->CreateView(640, 480, viewConfig, session);
    if (!view) {
        std::cerr << "Ultralight Free View creation failed\n";
        return 1;
    }

    AcceptanceListener listener;
    view->set_view_listener(&listener);
    view->set_load_listener(&listener);
    view->LoadURL("file:///acceptance.html");

    if (!PumpUntil(renderer.get(), [&] { return listener.domReady || listener.loadFailed; }, 400) ||
        listener.loadFailed) {
        std::cerr << "Ultralight Free acceptance document did not reach DOMReady\n";
        return 1;
    }

    std::string result;
    PumpUntil(renderer.get(), [&] {
        ultralight::String exception;
        const auto value = view->EvaluateScript(
            "window.__prisma_acceptance_done ? JSON.stringify({state:window.__prisma_acceptance,signals:window.__prisma_acceptance_signals}) : ''",
            &exception);
        result = ToUtf8(value);
        return exception.empty() && !result.empty();
    }, 250);

    view->set_view_listener(nullptr);
    view->set_load_listener(nullptr);
    const char* required[] = {
        "\"fetch\":\"blocked\"", "\"xhr\":\"blocked\"", "\"websocket\":\"blocked\"",
        "\"worker\":\"blocked\"", "\"script\":\"blocked\"", "\"style\":\"blocked\"",
        "\"image\":\"blocked\"", "\"font\":\"blocked\"", "\"media\":\"blocked\"",
        "\"frame\":\"blocked\"", "\"redirect\":\"blocked\"", "\"form\":\"blocked\"",
        "\"worker\":true", "\"form\":true",
    };
    for (const char* value : required) {
        if (!Contains(result, value)) {
            std::cerr << "Free CSP acceptance failed: missing " << value << " in " << result << "\n";
            return 1;
        }
    }
    if (probe.requests() != 0) {
        std::cerr << "Free CSP acceptance failed: remote request reached probe (" << probe.requests() << ")\n";
        return 1;
    }

    AcceptanceListener redirectListener;
    auto redirectView = renderer->CreateView(640, 480, viewConfig, session);
    if (!redirectView) {
        std::cerr << "Ultralight Free redirect View creation failed\n";
        return 1;
    }
    redirectView->set_view_listener(&redirectListener);
    redirectView->set_load_listener(&redirectListener);
    redirectView->LoadURL("file:///redirect.html");
    if (!PumpUntil(renderer.get(), [&] { return redirectListener.domReady || redirectListener.loadFailed; }, 400) ||
        redirectListener.loadFailed) {
        std::cerr << "Ultralight Free redirect document did not reach DOMReady\n";
        return 1;
    }
    std::string redirectResult;
    PumpUntil(renderer.get(), [&] {
        ultralight::String exception;
        const auto value = redirectView->EvaluateScript(
            "window.__prisma_redirect_done ? String(window.__prisma_redirect_done) : ''", &exception);
        redirectResult = ToUtf8(value);
        return exception.empty() && !redirectResult.empty() && redirectResult != "timeout";
    }, 250);
    redirectView->set_view_listener(nullptr);
    redirectView->set_load_listener(nullptr);
    if (redirectProbe.requests() != 1 || forbiddenProbe.requests() != 0 || redirectResult != "blocked") {
        std::cerr << "Free redirect acceptance failed: result=" << redirectResult
                  << " allowed_requests=" << redirectProbe.requests()
                  << " forbidden_requests=" << forbiddenProbe.requests() << "\n";
        return 1;
    }

    AcceptanceListener navigationListener;
    navigationListener.ConfigureTrustedNavigation("file:///navigation.html");
    auto navigationView = renderer->CreateView(640, 480, viewConfig, session);
    if (!navigationView) {
        std::cerr << "Ultralight Free navigation View creation failed\n";
        return 1;
    }
    navigationView->set_view_listener(&navigationListener);
    navigationView->set_load_listener(&navigationListener);
    navigationView->LoadURL("file:///navigation.html");
    PumpUntil(renderer.get(), [&] {
        return navigationListener.blockedNavigation || navigationListener.loadFailed;
    }, 400);
    navigationView->set_view_listener(nullptr);
    navigationView->set_load_listener(nullptr);
    if (!navigationListener.remoteNavigationObserved || !navigationListener.blockedNavigation || navigationProbe.requests() != 0) {
        std::cerr << "Free top-level navigation acceptance failed: begin=" << navigationListener.beginUrl
                  << " changed=" << navigationListener.changedUrl
                  << " blocked=" << navigationListener.blockedNavigation
                  << " requests=" << navigationProbe.requests() << "\n";
        return 1;
    }

    std::cout << "ultralight free network acceptance passed: CSP blocked remote surfaces, redirects were exercised, and trusted-document navigation was rejected before remote traffic\n";
    return 0;
}