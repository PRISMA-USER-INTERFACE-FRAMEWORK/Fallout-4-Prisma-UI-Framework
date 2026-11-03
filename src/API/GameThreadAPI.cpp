#include "PCH.h"
#include "API.h"

#include "PrismaUI/WebRuntime.h"
#include "PrismaUI/GameThreadDispatcher.h"
#include "PrismaUI/ModelPreview.h"
#include "Utils/ConflictChecker.h"
#include <intrin.h>
#pragma intrinsic(_ReturnAddress)

#include <exception>
#include <string>
#include <utility>
#include <atomic>
#include <chrono>

namespace {

void LogDispatchFailure(PrismaView view, const std::string& name, const char* api)
{
    static std::atomic<int64_t> lastLogMs{0};
    static std::atomic<uint32_t> suppressed{0};
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count();
    int64_t previous = lastLogMs.load(std::memory_order_relaxed);
    if (previous != 0 && now - previous < 1000) {
        suppressed.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    if (!lastLogMs.compare_exchange_strong(previous, now, std::memory_order_relaxed)) {
        suppressed.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    const uint32_t skipped = suppressed.exchange(0, std::memory_order_relaxed);
    if (skipped > 0) {
        logger::error("[{}] verified window-thread dispatcher unavailable; dropping view [{}] '{}' "
                      "callback ({} similar drops suppressed)",
                      api, view, name, skipped);
        return;
    }
    logger::error("[{}] verified window-thread dispatcher unavailable; dropping view [{}] '{}' callback",
                  api, view, name);
}

}

PrismaView PluginAPI::VerifiedPrismaUIInterface::CreateView(
    const char* htmlPath,
    PRISMA_UI_API::OnDomReadyCallback onDomReadyCallback) noexcept
{
    if (!htmlPath) return 0;

    const std::string owner = PrismaUI::ConflictChecker::OwnerOf(_ReturnAddress());

    PrismaUI::WebRuntime::TryInstallGameInput();
    if (onDomReadyCallback && !PrismaUI::GameThreadDispatcher::IsReady()) {
        logger::critical(
            "CreateView: refusing callback-bearing view '{}' for '{}' because the verified "
            "Fallout window-thread dispatcher is not ready",
            htmlPath, owner);
        return 0;
    }

    if (!PrismaUI::WebRuntime::EnsureLoaded()) {
        logger::critical("CreateView: PrismaUI_F4's Host backend failed to initialize -- cannot create "
                         "view '{}'. See the [WebRuntime] critical line above for the real cause.", htmlPath);
        return 0;
    }

    std::function<void(uint64_t)> webDomReady;
    if (onDomReadyCallback) {
        webDomReady = [onDomReadyCallback](uint64_t id) {
            bool queued = false;
            try {
                queued = PrismaUI::GameThreadDispatcher::Dispatch(
                    [onDomReadyCallback, id]() { onDomReadyCallback(id); }, id);
            } catch (const std::exception& e) {
                logger::error("CreateView: failed to queue OnDomReady for view [{}]: {}", id, e.what());
                return;
            } catch (...) {
                logger::error("CreateView: failed to queue OnDomReady for view [{}]", id);
                return;
            }
            if (!queued) {
                logger::error("CreateView: verified window-thread queue rejected OnDomReady for view [{}]", id);
            }
        };
    }

    const auto id = PrismaUI::WebRuntime::CreateView(htmlPath, webDomReady, owner.c_str());
    logger::info("CreateView (verified Ultralight): path='{}' -> view [{}] owner='{}'", htmlPath, id, owner);
    PrismaUI::ModelPreview::NoteViewCreated();
    return id;
}

bool PluginAPI::PrismaUIInterface::DispatchToGameThread(
    PRISMA_UI_API::GameThreadTaskCallback callback,
    void* userdata) noexcept
{
    if (!callback) {
        logger::warn("[V11] DispatchToGameThread: null callback");
        return false;
    }

    try {
        return PrismaUI::GameThreadDispatcher::Dispatch([callback, userdata]() {
            callback(userdata);
        });
    } catch (const std::exception& e) {
        logger::error("[V11] DispatchToGameThread: failed to queue callback: {}", e.what());
        return false;
    } catch (...) {
        logger::error("[V11] DispatchToGameThread: failed to queue callback");
        return false;
    }
}

bool PluginAPI::PrismaUIInterface::IsGameThread() noexcept
{
    return PrismaUI::GameThreadDispatcher::IsGameThread();
}

bool PluginAPI::PrismaUIInterface::BindGameThreadUIEvent(
    PrismaView view,
    const char* functionName,
    PRISMA_UI_API::GameThreadUIEventCallback callback,
    void* userdata) noexcept
{
    if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view) || !functionName || !functionName[0] || !callback) {
        logger::warn("[V11] BindGameThreadUIEvent: invalid args view={} fn={}",
                     view, functionName ? functionName : "null");
        return false;
    }
    if (!PrismaUI::WebRuntime::IsActive() || !PrismaUI::WebRuntime::IsValid(view)) {
        logger::warn("[V11] BindGameThreadUIEvent: view [{}] is not live", view);
        return false;
    }
    if (!PrismaUI::GameThreadDispatcher::IsReady()) {
        logger::warn("[V11] BindGameThreadUIEvent: window-thread dispatcher is not ready");
        return false;
    }

    const std::string name(functionName);
    PrismaUI::WebRuntime::RegisterJSListener(
        view,
        functionName,
        [callback, userdata, view, name](const std::string& argument) {
            std::string payload = argument;
            bool queued = false;
            try {
                queued = PrismaUI::GameThreadDispatcher::Dispatch(
                    [callback, userdata, view, name, payload = std::move(payload)]() {
                        logger::debug("[V11] window-thread UI event: view [{}] '{}' data='{}'",
                                      view, name, payload);
                        callback(payload.c_str(), userdata);
                    },
                    view);
            } catch (const std::exception& e) {
                logger::error("[V11] BindGameThreadUIEvent: queue failed for view [{}] '{}': {}",
                              view, name, e.what());
                return;
            } catch (...) {
                logger::error("[V11] BindGameThreadUIEvent: queue failed for view [{}] '{}'",
                              view, name);
                return;
            }
            if (!queued) {
                LogDispatchFailure(view, name, "V11");
            }
        });

    logger::info("[V11] BindGameThreadUIEvent: view [{}] registered '{}'", view, name);
    return true;
}

void PluginAPI::VerifiedPrismaUIInterface::BindUIEvent(
    PrismaView view,
    const char* functionName,
    PRISMA_UI_API::JSListenerCallback callback) noexcept
{
    if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view) || !functionName || !functionName[0] || !callback) {
        logger::warn("[V4] BindUIEvent: invalid args view={} fn={}",
                     view, functionName ? functionName : "null");
        return;
    }
    if (!PrismaUI::WebRuntime::IsActive() || !PrismaUI::WebRuntime::IsValid(view)) {
        logger::warn("[V4] BindUIEvent: view [{}] is not live", view);
        return;
    }
    if (!PrismaUI::GameThreadDispatcher::IsReady()) {
        logger::critical("[V4] BindUIEvent: window-thread dispatcher is not ready; not registering '{}'",
                         functionName);
        return;
    }

    const std::string name(functionName);
    PrismaUI::WebRuntime::RegisterJSListener(
        view,
        functionName,
        [callback, view, name](const std::string& argument) {
            std::string payload = argument;
            bool queued = false;
            try {
                queued = PrismaUI::GameThreadDispatcher::Dispatch(
                    [callback, view, name, payload = std::move(payload)]() {
                        logger::debug("[V4] BindUIEvent fired on verified window thread: view [{}] '{}' data='{}'",
                                      view, name, payload);
                        callback(payload.c_str());
                    },
                    view);
            } catch (const std::exception& e) {
                logger::error("[V4] BindUIEvent: queue failed for view [{}] '{}': {}",
                              view, name, e.what());
                return;
            } catch (...) {
                logger::error("[V4] BindUIEvent: queue failed for view [{}] '{}'", view, name);
                return;
            }
            if (!queued) {
                LogDispatchFailure(view, name, "V4");
            }
        });

    logger::info("[V4] BindUIEvent: view [{}] registered verified window-thread listener '{}'", view, name);
}
