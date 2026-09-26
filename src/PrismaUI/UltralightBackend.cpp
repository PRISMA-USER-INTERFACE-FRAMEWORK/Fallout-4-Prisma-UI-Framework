#include "UltralightBackend.h"

#include "DevToolsConfig.h"
#include "FreezeDiagnostics.h"
#include "GPU/FalloutGPUDriver.h"
#include "GPU/GenerationDrainState.h"
#include "PrismaFileSystem.h"
#include "SystemClipboard.h"
#include "UltralightRuntime.h"

#pragma warning(push)
#pragma warning(disable : 4100)
#include <Ultralight/Ultralight.h>
#pragma warning(pop)

#include <AppCore/Platform.h>

#include <windows.h>

#include <chrono>
#include <exception>
#include <utility>

namespace PrismaUI::WebRuntimeUltralight {

    static_assert(sizeof(ultralight::Config) > 0,
                  "Ultralight 1.4 SDK headers not on the include path (external/ultralight-1.4.0/include)");

    namespace {

        constexpr uint32_t kDefaultWidth = 1280;
        constexpr uint32_t kDefaultHeight = 720;

    }

    UltralightBackend::UltralightBackend() : documentCallbacks_(*this), inspector_(*this) {}

    UltralightBackend::~UltralightBackend() {
        Shutdown();
    }

    void UltralightBackend::Initialize(ID3D11Device* device, ID3D11DeviceContext* immediate, HWND window,
                                       PrismaUI::Web::ReadyCallback onReady) {
        std::unique_lock lock(stateMutex_);
        if (initialized_) {
            lock.unlock();
            if (onReady) onReady(false, "Ultralight backend can only be initialized once");
            return;
        }
        initialized_ = true;
        acceptingFrames_ = true;
        ready_ = false;
        terminalGpuFailure_ = false;
        window_ = window;
        if (device) device_ = device;
        if (immediate) immediateContext_ = immediate;
        width_ = kDefaultWidth;
        height_ = kDefaultHeight;
        if (window_) {
            RECT rect{};
            if (::GetClientRect(window_, &rect) && rect.right > 0 && rect.bottom > 0) {
                width_ = static_cast<uint32_t>(rect.right);
                height_ = static_cast<uint32_t>(rect.bottom);
            }
        }
        dispatchQueue_.Start();
        initializationDone_ = false;
        initializationResolved_ = false;
        auto callback = onReady;
        try {
            ownerThread_ = std::thread(
                [this, callback = std::move(callback)]() mutable { OwnerMain(std::move(callback)); });
        } catch (const std::exception& exception) {
            acceptingFrames_ = false;
            ready_ = false;
            dispatchQueue_.Stop();
            initializationDone_ = true;
            initializationSuccess_ = false;
            initializationMessage_ = exception.what();
            const auto message = initializationMessage_;
            lock.unlock();
            if (onReady) onReady(false, message);
            return;
        }
        bool deliverTimeout = false;
        std::string timeoutMessage;
        if (!initializationCv_.wait_for(lock, std::chrono::seconds(20),
                                        [this] { return initializationDone_; })) {
            if (!initializationResolved_) {
                initializationResolved_ = true;
                dispatchQueue_.Stop();
                acceptingFrames_ = false;
                ready_ = false;
                initializationDone_ = true;
                initializationSuccess_ = false;
                initializationMessage_ = "Ultralight backend initialization timed out";
                timeoutMessage = initializationMessage_;
                deliverTimeout = true;
                logger::error("[WebRuntime] backend initialization timed out; failing closed");
            }
        }
        lock.unlock();
        if (deliverTimeout && onReady) onReady(false, timeoutMessage);
    }

    void UltralightBackend::Shutdown() {
        if (shutdownStarted_.exchange(true, std::memory_order_acq_rel)) return;
        CancelNativeGamepad(0, true);
        {
            std::lock_guard lock(stateMutex_);
            acceptingFrames_ = false;
            ready_ = false;
            dispatchQueue_.Stop();
        }
        stateCv_.notify_all();
        dispatchQueue_.NotifyAll();
        if (ownerThread_.joinable() && ownerThread_.get_id() != std::this_thread::get_id()) {
            logger::info("[WebRuntime] Shutdown: joining owner thread (current op: {})",
                         currentOp_.load(std::memory_order_acquire));
            FreezeDiagnostics::BeginShutdownJoin();
            struct JoinGuard {
                ~JoinGuard() { FreezeDiagnostics::EndShutdownJoin(); }
            } joinGuard;
            constexpr int kOwnerExitPollLimit = 50;
            int waited = 0;
            while (!ownerExited_.load(std::memory_order_acquire) && waited < kOwnerExitPollLimit) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                ++waited;
            }
            if (ownerExited_.load(std::memory_order_acquire)) {
                ownerThread_.join();
            } else {
                logger::error("[WebRuntime] Shutdown: owner thread did not exit within 5s (op: {}); "
                              "leaving the process-lifetime backend resident rather than detaching a "
                              "thread that still references it; skipping teardown",
                              currentOp_.load(std::memory_order_acquire));
                return;
            }
        }
        {
            std::lock_guard lock(stateMutex_);
            completedGeneration_ = 0;
            activeGeneration_ = 0;
            published_.clear();
            immediateContext_.Reset();
            device_.Reset();
            window_ = nullptr;
        }
        viewManager_.ClearPublished();
    }

    bool UltralightBackend::IsReady() const {
        std::lock_guard lock(stateMutex_);
        return ready_;
    }

    void UltralightBackend::Resize(uint32_t width, uint32_t height) {
        if (!width || !height) return;
        Dispatch([this, width, height] {
            width_ = width;
            height_ = height;
            viewManager_.ForEach([this](PrismaUI::Web::ViewId, ViewRecord& record) {
                if (record.viewportSized && record.view) record.view->Resize(width_, height_);
            });
            inspector_.SetViewport(width_, height_);
        });
    }

    void UltralightBackend::OwnerMain(PrismaUI::Web::ReadyCallback callback) {
        dispatchQueue_.SetOwnerThread();
        struct OwnerExitGuard {
            std::atomic_bool& exited;
            std::atomic<const char*>& op;
            ~OwnerExitGuard() {
                op.store("exited", std::memory_order_release);
                exited.store(true, std::memory_order_release);
            }
        } ownerExitGuard{ownerExited_, currentOp_};
        bool success = false;
        std::string message;
        bool platformConfigured = false;
        try {
            if (!runtime_.Load(message)) {
                if (message.empty()) message = "Ultralight runtime DLLs were not found";
            } else {
                ultralight::Config config;
                config.cache_path = "";
                config.resource_path_prefix = "resources/";
                auto& platform = ultralight::Platform::instance();
                platformConfigured = true;
                platform.set_config(config);
                fileSystem_ = std::make_unique<PrismaFileSystem>(UltralightViewRoot());
                platform.set_file_system(fileSystem_.get());
                platform.set_font_loader(ultralight::GetPlatformFontLoader());
                platform.set_clipboard(&SystemClipboard::UltralightClipboard());
                const bool allowAccelerated = false;
                const bool runtimeAcceptanceBuild = PRISMA_ACCELERATED_RUNTIME_ACCEPTANCE != 0;
                const bool acceleratedForRuntimeAcceptance = allowAccelerated || runtimeAcceptanceBuild;
                if (acceleratedForRuntimeAcceptance) {
                    logger::warn("[WebRuntime] [GPU] INTERNAL accelerated-v2 runtime acceptance build enabled");
                } else {
                    logger::info("[WebRuntime] [GPU] Ultralight CPU backend (accelerated disabled pending #674)");
                }
                accelerated_ = acceleratedForRuntimeAcceptance && device_ && immediateContext_ &&
                               gpuContext_.Initialize(device_.Get(), immediateContext_.Get(), runtimeAcceptanceBuild);
                if (accelerated_) {
                    gpuDriver_ = std::make_unique<PrismaUI::GPU::FalloutGPUDriver>(&gpuContext_);
                }
                if (!accelerated_) {
                    gpuDriver_.reset();
                    gpuContext_.Reset();
                }
                platform.set_gpu_driver(accelerated_ ? gpuDriver_.get() : nullptr);
                renderer_ = ultralight::Renderer::Create();
                session_ = renderer_ ? renderer_->CreateSession(false, "prisma")
                                     : ultralight::RefPtr<ultralight::Session>{};
                success = renderer_ && session_;
                message = success ? (accelerated_ ? "Ultralight GPU backend ready"
                                                   : "Ultralight CPU backend ready")
                                  : "Ultralight Renderer::Create failed";
                if (success) {
                    const auto iniPath = Utils::PluginIniPath();
                    if (DevToolsConfig::Enabled(iniPath.wstring()))
                        logger::info("[WebRuntime] [DevTools] local inspector enabled; press F12 in-game");
                }
            }
        } catch (const std::exception& exception) {
            message = exception.what();
        } catch (...) {
            message = "Ultralight initialization failed";
        }
        bool deliver = false;
        {
            std::lock_guard lock(stateMutex_);
            initializationMessage_ = message;
            initializationDone_ = true;
            if (!initializationResolved_) {
                initializationResolved_ = true;
                initializationSuccess_ = success;
                ready_ = success;
                deliver = true;
            } else {
                success = false;
                ready_ = false;
            }
        }
        initializationCv_.notify_all();
        if (deliver && callback) callback(success, message);
        if (!success) {
            std::lock_guard lock(stateMutex_);
            acceptingFrames_ = false;
            dispatchQueue_.Stop();
            if (platformConfigured) {
                auto& platform = ultralight::Platform::instance();
                platform.set_gpu_driver(nullptr);
                platform.set_font_loader(nullptr);
                platform.set_file_system(nullptr);
                platform.set_clipboard(nullptr);
            }
            gpuDriver_.reset();
            gpuContext_.Reset();
            fileSystem_.reset();
            return;
        }

        while (true) {
            {
                currentOp_.store("idle", std::memory_order_release);
                FreezeDiagnostics::MarkIdle(FreezeDiagnostics::Lane::UltralightOwner);
                dispatchQueue_.WaitForWork();
            }
            if (dispatchQueue_.IsStopping()) break;
            currentOp_.store("tasks", std::memory_order_release);
            FreezeDiagnostics::MarkStage(FreezeDiagnostics::Lane::UltralightOwner,
                                         FreezeDiagnostics::Stage::DispatchTask);
            const auto taskStart = std::chrono::steady_clock::now();
            for (int taskCount = 0; taskCount < 16; ++taskCount) {
                UltralightDispatchQueue::Task task;
                if (!dispatchQueue_.TryPop(task)) break;
                try {
                    task();
                } catch (...) {
                }
                if (std::chrono::steady_clock::now() - taskStart >= std::chrono::milliseconds(2)) break;
            }
            if (dispatchQueue_.IsStopping()) break;
            if (renderer_ && dispatchQueue_.FrameRequested()) {
                bool mayRender;
                {
                    std::unique_lock lock(stateMutex_);
                    mayRender = dispatchQueue_.IsStopping() || !PrismaUI::GPU::GenerationDrain::OwnerMayWaitForRetirement(
                                                                            completedGeneration_, activeGeneration_, terminalGpuFailure_);
                    if (!mayRender) {
                        stateCv_.wait_for(lock, std::chrono::milliseconds(2), [this] {
                            return dispatchQueue_.IsStopping() || !PrismaUI::GPU::GenerationDrain::OwnerMayWaitForRetirement(
                                                                        completedGeneration_, activeGeneration_, terminalGpuFailure_);
                        });
                        mayRender = dispatchQueue_.IsStopping() || !PrismaUI::GPU::GenerationDrain::OwnerMayWaitForRetirement(
                                                                            completedGeneration_, activeGeneration_, terminalGpuFailure_);
                    }
                }
                if (dispatchQueue_.IsStopping()) break;
                if (mayRender && dispatchQueue_.ConsumeFrameRequest()) {
                    try {
                        currentOp_.store("Renderer::Update", std::memory_order_release);
                        FreezeDiagnostics::MarkStage(FreezeDiagnostics::Lane::UltralightOwner,
                                                     FreezeDiagnostics::Stage::UltralightUpdate);
                        renderer_->Update();
                        currentOp_.store("Renderer::RefreshDisplay", std::memory_order_release);
                        FreezeDiagnostics::MarkStage(FreezeDiagnostics::Lane::UltralightOwner,
                                                     FreezeDiagnostics::Stage::UltralightRefreshDisplay);
                        renderer_->RefreshDisplay(0);
                        currentOp_.store("Renderer::Render", std::memory_order_release);
                        FreezeDiagnostics::MarkStage(FreezeDiagnostics::Lane::UltralightOwner,
                                                     FreezeDiagnostics::Stage::UltralightRender);
                        renderer_->Render();
                        currentOp_.store("PublishFrames", std::memory_order_release);
                        PublishFrames();
                        FreezeDiagnostics::NoteRenderSuccess();
                    } catch (const std::exception& exception) {
                        std::lock_guard lock(stateMutex_);
                        ready_ = false;
                        acceptingFrames_ = false;
                        dispatchQueue_.Stop();
                        initializationMessage_ = exception.what();
                    } catch (...) {
                        std::lock_guard lock(stateMutex_);
                        ready_ = false;
                        acceptingFrames_ = false;
                        dispatchQueue_.Stop();
                        initializationMessage_ = "Ultralight render loop failed";
                    }
                    stateCv_.notify_all();
                }
            }
        }
        currentOp_.store("teardown", std::memory_order_release);
        viewManager_.ClearRecords();
        inspector_.Clear();
        session_ = nullptr;
        renderer_ = nullptr;
        if (platformConfigured) {
            auto& platform = ultralight::Platform::instance();
            platform.set_gpu_driver(nullptr);
            platform.set_font_loader(nullptr);
            platform.set_file_system(nullptr);
            platform.set_clipboard(nullptr);
        }
        gpuDriver_.reset();
        gpuContext_.Reset();
        fileSystem_.reset();
    }

    PrismaUI::Web::IWebBackend& Backend() {
        static UltralightBackend* backend = new UltralightBackend();
        return *backend;
    }

}
