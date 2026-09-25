#include "UltralightBackend.h"

#include "FreezeDiagnostics.h"
#include "GPU/FalloutGPUDriver.h"
#include "GPU/GenerationDrainState.h"

#include <chrono>

namespace PrismaUI::WebRuntimeUltralight {

    namespace {

        const char* HResultReason(HRESULT result) noexcept {
            switch (result) {
                case DXGI_ERROR_DEVICE_REMOVED:
                    return "DEVICE_REMOVED";
                case DXGI_ERROR_DEVICE_RESET:
                    return "DEVICE_RESET";
                case DXGI_ERROR_INVALID_CALL:
                    return "INVALID_CALL";
                case E_OUTOFMEMORY:
                    return "OUT_OF_MEMORY";
                default:
                    return "UNKNOWN";
            }
        }

    }

    uint64_t UltralightBackend::DeviceEpoch() const noexcept {
        std::lock_guard lock(stateMutex_);
        return accelerated_ ? deviceEpoch_ : 0;
    }

    bool UltralightBackend::PresentationStateValid() const noexcept {
        std::lock_guard lock(stateMutex_);
        return !accelerated_ || (!lifecycle_.Dead() && !lifecycle_.ResizePending() && !terminalGpuFailure_);
    }

    void UltralightBackend::BeginResize(IDXGISwapChain* swapChain) {
        std::lock_guard lock(stateMutex_);
        if (!accelerated_ || !gpuDriver_) return;
        lifecycle_.BeginResize();
        resizeSwapChain_ = swapChain;
        InvalidateAcceleratedPresentationLocked();
    }

    void UltralightBackend::CompleteResize(IDXGISwapChain* swapChain, HRESULT result, uint32_t, uint32_t) {
        std::lock_guard lock(stateMutex_);
        if (!accelerated_ || !gpuDriver_ || !lifecycle_.ResizePending()) return;
        if (FAILED(result) || !gpuContext_.PrepareImmediateDevice(immediateContext_.Get())) {
            lifecycle_.CompleteResize(false, {});
            resizeSwapChain_ = nullptr;
            return;
        }
        const auto inputs = LifecycleInputs(immediateContext_.Get(), swapChain ? swapChain : resizeSwapChain_);
        if (lifecycle_.CompleteResize(true, inputs) !=
                PrismaUI::GPU::AcceleratedDeviceLifecycle::Transition::Changed ||
            !CommitAcceleratedLifecycleLocked()) {
            lifecycle_.MarkDeviceLost();
            terminalGpuFailure_ = true;
            acceptingFrames_ = false;
            ready_ = false;
        }
        resizeSwapChain_ = nullptr;
    }

    PrismaUI::Web::PresentLease UltralightBackend::BeginPresentFrame(
        ID3D11DeviceContext* immediate, IDXGISwapChain* swapChain) {
        FreezeDiagnostics::MarkPresent(FreezeDiagnostics::Stage::BeginPresentFrame);
        std::unique_lock lock(stateMutex_);
        FreezeDiagnostics::SetGpuState(completedGeneration_, activeGeneration_, dispatchQueue_.FrameRequested());
        if (!immediate || activeGeneration_ != 0) return {};
        const auto beforeEpoch = deviceEpoch_;
        if (accelerated_ && !ObserveAcceleratedLifecycleLocked(immediate, swapChain)) return {};
        if (!PrismaUI::GPU::GenerationDrain::CanBeginPresent(acceptingFrames_, ready_, terminalGpuFailure_)) return {};
        if (beforeEpoch != deviceEpoch_) dispatchQueue_.RequestFrame();
        if (completedGeneration_ == 0) {
            dispatchQueue_.RequestFrame();
            dispatchQueue_.NotifyOne();
            stateCv_.wait_for(lock, std::chrono::milliseconds(2), [this] {
                return completedGeneration_ != 0 || !acceptingFrames_ || dispatchQueue_.IsStopping();
            });
        }
        if (!PrismaUI::GPU::GenerationDrain::CanBeginPresent(acceptingFrames_, ready_, terminalGpuFailure_) ||
            completedGeneration_ == 0 || activeGeneration_ != 0)
            return {};

        PrismaUI::GPU::DrainResult drain;
        const auto activation = PrismaUI::GPU::GenerationDrain::ActivatePublished(
            completedGeneration_, activeGeneration_, accelerated_,
            gpuDriver_ && immediate == immediateContext_.Get(),
            [this, &drain] {
                FreezeDiagnostics::MarkPresent(FreezeDiagnostics::Stage::DrainGpuGeneration);
                gpuDriver_->SetDiagnosticGeneration(completedGeneration_);
                drain = gpuDriver_->DrainPendingGeneration();
                return static_cast<bool>(drain);
            });
        if (activation == PrismaUI::GPU::GenerationDrain::Activation::DrainFailed) {
            logger::error("[WebRuntime] Ultralight GPU generation drain failed: generation={} hr={:#010x} reason={}",
                          completedGeneration_, static_cast<unsigned long>(drain.result), HResultReason(drain.result));
            if (drain.result == DXGI_ERROR_DEVICE_REMOVED || drain.result == DXGI_ERROR_DEVICE_RESET) {
                lifecycle_.MarkDeviceLost();
                InvalidateAcceleratedPresentationLocked();
            } else {
                PrismaUI::GPU::GenerationDrain::EnterTerminalFailure(
                    completedGeneration_, activeGeneration_, acceptingFrames_, ready_, terminalGpuFailure_);
                dispatchQueue_.Stop();
            }
            lock.unlock();
            stateCv_.notify_all();
            dispatchQueue_.NotifyAll();
            return {};
        }
        if (activation != PrismaUI::GPU::GenerationDrain::Activation::Activated) return {};

        if (accelerated_) {
            bool resourcesReady = true;
            for (auto& [_, frame] : published_) {
                if (frame.generation != completedGeneration_ || frame.deviceEpoch != deviceEpoch_ ||
                    frame.textureId == 0) {
                    resourcesReady = false;
                    break;
                }
                const auto snapshot = gpuDriver_->AcquirePresentationTextureSnapshot(frame.textureId);
                if (!snapshot.srv || snapshot.width != frame.textureWidth ||
                    snapshot.height != frame.textureHeight || snapshot.format == DXGI_FORMAT_UNKNOWN) {
                    resourcesReady = false;
                    break;
                }
                frame.srv = snapshot.srv;
                frame.format = snapshot.format;
            }
            if (!resourcesReady) {
                logger::error("[WebRuntime] accelerated frame resource handoff failed: generation={}",
                              completedGeneration_);
                PrismaUI::GPU::GenerationDrain::EnterTerminalFailure(
                    completedGeneration_, activeGeneration_, acceptingFrames_, ready_, terminalGpuFailure_);
                dispatchQueue_.Stop();
                lock.unlock();
                stateCv_.notify_all();
                dispatchQueue_.NotifyAll();
                return {};
            }
        }

        if (!accelerated_) {
            for (auto& [_, frame] : published_) {
                if (frame.generation != completedGeneration_ || frame.width == 0 || frame.height == 0 ||
                    frame.pixels.empty())
                    continue;
                D3D11_TEXTURE2D_DESC desc{};
                if (frame.texture) frame.texture->GetDesc(&desc);
                if (!frame.texture || desc.Width != frame.width || desc.Height != frame.height ||
                    desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM) {
                    D3D11_TEXTURE2D_DESC textureDesc{};
                    textureDesc.Width = frame.width;
                    textureDesc.Height = frame.height;
                    textureDesc.MipLevels = 1;
                    textureDesc.ArraySize = 1;
                    textureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                    textureDesc.SampleDesc.Count = 1;
                    textureDesc.Usage = D3D11_USAGE_DEFAULT;
                    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                    frame.texture.Reset();
                    frame.srv.Reset();
                    if (!device_ || FAILED(device_->CreateTexture2D(&textureDesc, nullptr, &frame.texture)) ||
                        FAILED(device_->CreateShaderResourceView(frame.texture.Get(), nullptr, &frame.srv))) {
                        frame.texture.Reset();
                        frame.srv.Reset();
                        continue;
                    }
                    frame.uploadedGeneration = 0;
                }
                if (frame.uploadedGeneration != frame.contentGeneration) {
                    immediate->UpdateSubresource(frame.texture.Get(), 0, nullptr, frame.pixels.data(),
                                                 frame.rowBytes, 0);
                    frame.uploadedGeneration = frame.contentGeneration;
                }
            }
        }
        return MakePresentLease(activeGeneration_, deviceEpoch_);
    }

    PrismaUI::Web::RenderTargetSnapshot UltralightBackend::ViewRenderTarget(
        PrismaUI::Web::ViewId view, uint64_t generation) {
        std::lock_guard lock(stateMutex_);
        const auto it = published_.find(view);
        if (it == published_.end() || it->second.generation != generation ||
            (accelerated_ && it->second.deviceEpoch != deviceEpoch_) || !it->second.srv)
            return {};
        PrismaUI::Web::RenderTargetSnapshot snapshot;
        snapshot.srv = it->second.srv;
        snapshot.viewportWidth = it->second.width;
        snapshot.viewportHeight = it->second.height;
        snapshot.textureWidth = it->second.textureWidth;
        snapshot.textureHeight = it->second.textureHeight;
        snapshot.format = it->second.format;
        snapshot.uvLeft = it->second.uvLeft;
        snapshot.uvTop = it->second.uvTop;
        snapshot.uvRight = it->second.uvRight;
        snapshot.uvBottom = it->second.uvBottom;
        snapshot.publishGeneration = generation;
        snapshot.contentGeneration = it->second.contentGeneration;
        snapshot.deviceEpoch = it->second.deviceEpoch;
        return snapshot;
    }

    void UltralightBackend::EndPresentFrame(uint64_t generation, uint64_t epoch) noexcept {
        {
            std::lock_guard lock(stateMutex_);
            if (!activeGeneration_ || activeGeneration_ != generation || epoch != deviceEpoch_) {
                logger::warn("[WebRuntime] rejected invalid Ultralight present lease release: {}", generation);
                return;
            }
            activeGeneration_ = 0;
            completedGeneration_ = 0;
            dispatchQueue_.RequestFrame();
        }
        FreezeDiagnostics::SetGpuState(0, 0, true);
        stateCv_.notify_all();
        dispatchQueue_.NotifyOne();
    }

    PrismaUI::GPU::AcceleratedLifecycleInputs UltralightBackend::LifecycleInputs(
        ID3D11DeviceContext* immediate, IDXGISwapChain* swapChain) const noexcept {
        return {reinterpret_cast<std::uintptr_t>(gpuContext_.Device()),
                reinterpret_cast<std::uintptr_t>(immediate), reinterpret_cast<std::uintptr_t>(swapChain)};
    }

    void UltralightBackend::InvalidateAcceleratedPresentationLocked() noexcept {
        published_.clear();
        completedGeneration_ = 0;
        activeGeneration_ = 0;
        dispatchQueue_.RequestFrame();
    }

    bool UltralightBackend::CommitAcceleratedLifecycleLocked() {
        deviceEpoch_ = lifecycle_.Epoch();
        if (!gpuDriver_ || !gpuDriver_->RebuildForDeviceEpoch(deviceEpoch_)) return false;
        device_ = gpuContext_.Device();
        return true;
    }

    bool UltralightBackend::ObserveAcceleratedLifecycleLocked(
        ID3D11DeviceContext* immediate, IDXGISwapChain* swapChain) {
        if (!gpuDriver_ || !gpuContext_.PrepareImmediateDevice(immediate)) {
            lifecycle_.MarkDeviceLost();
            return false;
        }
        immediateContext_ = immediate;
        const auto transition = lifecycle_.Observe(LifecycleInputs(immediate, swapChain));
        if (transition == PrismaUI::GPU::AcceleratedDeviceLifecycle::Transition::Changed) {
            InvalidateAcceleratedPresentationLocked();
            if (!CommitAcceleratedLifecycleLocked()) {
                lifecycle_.MarkDeviceLost();
                terminalGpuFailure_ = true;
                acceptingFrames_ = false;
                ready_ = false;
                return false;
            }
        }
        return lifecycle_.Accepts(deviceEpoch_, LifecycleInputs(immediate, swapChain));
    }

}
