#include "OffscreenFrames.h"

#include <d3d11.h>
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "IWebBackend.h"
#include "OffscreenFrameState.h"
#include "Utils/ModulePath.h"

namespace PrismaUI::OffscreenFrames {
    namespace {

        struct StableOffscreenFrame {
            Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;

            uint64_t epoch = 0;
            uint64_t diagFrames = 0;

            uint64_t lastContentGen = 0;
            bool haveContent = false;
        };
        std::mutex g_offscreenMutex;
        std::map<ViewId, StableOffscreenFrame> g_offscreenFrames;
        uint64_t g_nextOffscreenEpoch = 1;

        uint64_t NextOffscreenEpochLocked() noexcept {
            uint64_t epoch = g_nextOffscreenEpoch++;
            if (epoch == 0) epoch = g_nextOffscreenEpoch++;
            return epoch;
        }

        bool ResolveSourceCoordinate(float uv, uint32_t extent, uint32_t& coordinate) {
            const double scaled = static_cast<double>(uv) * static_cast<double>(extent);
            const long long rounded = std::llround(scaled);
            if (rounded < 0 || rounded > static_cast<long long>(extent) ||
                std::abs(scaled - static_cast<double>(rounded)) > 0.0001) {
                return false;
            }
            coordinate = static_cast<uint32_t>(rounded);
            return true;
        }

        struct PixelStats {
            bool mapped = false;
            int nonBlack = 0;
            int sampled = 0;
            uint8_t maxR = 0, maxG = 0, maxB = 0, maxA = 0;
            uint8_t centreR = 0, centreG = 0, centreB = 0, centreA = 0;
        };

        PixelStats ProbeTexture(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* src) {
            PixelStats st;
            if (!src) return st;
            D3D11_TEXTURE2D_DESC d{};
            src->GetDesc(&d);

            D3D11_TEXTURE2D_DESC sd = d;
            sd.Usage = D3D11_USAGE_STAGING;
            sd.BindFlags = 0;
            sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            sd.MiscFlags = 0;
            Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
            if (FAILED(device->CreateTexture2D(&sd, nullptr, &staging))) return st;

            context->CopyResource(staging.Get(), src);

            D3D11_MAPPED_SUBRESOURCE m{};
            if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &m))) return st;
            st.mapped = true;
            const auto* base = static_cast<const uint8_t*>(m.pData);
            for (UINT y = 0; y < d.Height; y += 16) {
                const auto* row = base + static_cast<size_t>(y) * m.RowPitch;
                for (UINT x = 0; x < d.Width; x += 16) {
                    const uint8_t* p = row + static_cast<size_t>(x) * 4;
                    ++st.sampled;
                    if (p[0] > 8 || p[1] > 8 || p[2] > 8) ++st.nonBlack;
                    st.maxB = std::max(st.maxB, p[0]);
                    st.maxG = std::max(st.maxG, p[1]);
                    st.maxR = std::max(st.maxR, p[2]);
                    st.maxA = std::max(st.maxA, p[3]);
                }
            }
            const auto* c =
                base + static_cast<size_t>(d.Height / 2) * m.RowPitch + static_cast<size_t>(d.Width / 2) * 4;
            st.centreB = c[0];
            st.centreG = c[1];
            st.centreR = c[2];
            st.centreA = c[3];
            context->Unmap(staging.Get(), 0);
            return st;
        }

        void DumpTextureBMP(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* src,
                            const std::string& path) {
            if (!src) return;
            D3D11_TEXTURE2D_DESC d{};
            src->GetDesc(&d);
            D3D11_TEXTURE2D_DESC sd = d;
            sd.Usage = D3D11_USAGE_STAGING;
            sd.BindFlags = 0;
            sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            sd.MiscFlags = 0;
            Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
            if (FAILED(device->CreateTexture2D(&sd, nullptr, &staging))) return;
            context->CopyResource(staging.Get(), src);
            D3D11_MAPPED_SUBRESOURCE m{};
            if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &m))) return;

            const uint32_t w = d.Width, h = d.Height;
            const uint32_t pixBytes = w * h * 4;
            const uint32_t fileSize = 54 + pixBytes;
            std::vector<uint8_t> buf(fileSize, 0);
            uint8_t* f = buf.data();
            f[0] = 'B';
            f[1] = 'M';
            std::memcpy(f + 2, &fileSize, 4);
            const uint32_t off = 54;
            std::memcpy(f + 10, &off, 4);
            const uint32_t hdr = 40;
            std::memcpy(f + 14, &hdr, 4);
            std::memcpy(f + 18, &w, 4);
            const int32_t negH = -static_cast<int32_t>(h);
            std::memcpy(f + 22, &negH, 4);
            const uint16_t planes = 1, bpp = 32;
            std::memcpy(f + 26, &planes, 2);
            std::memcpy(f + 28, &bpp, 2);
            std::memcpy(f + 34, &pixBytes, 4);
            const auto* base = static_cast<const uint8_t*>(m.pData);
            for (uint32_t y = 0; y < h; ++y) {
                std::memcpy(f + 54 + static_cast<size_t>(y) * w * 4, base + static_cast<size_t>(y) * m.RowPitch,
                            static_cast<size_t>(w) * 4);
            }
            context->Unmap(staging.Get(), 0);
            if (std::ofstream out{path, std::ios::binary}) out.write(reinterpret_cast<const char*>(f), fileSize);
            logger::info("[DIAG] dumped web source frame -> {}", path);
        }

        void UpdateImpl(Backend backend, uint64_t presentGeneration, ID3D11Device* device,
                        ID3D11DeviceContext* context) {
            if (!backend || !device || !context || presentGeneration == 0) return;

            static std::vector<ViewId> views;
            views.clear();
            {
                std::lock_guard lock(g_offscreenMutex);
                views.reserve(g_offscreenFrames.size());
                for (const auto& [view, _] : g_offscreenFrames) views.push_back(view);
            }

            static const bool s_probeEnabled = [] {
                const auto ini = PrismaUI::Utils::PluginIniPath().string();
                const bool on = GetPrivateProfileIntA("Debug", "TextureProbe", 0, ini.c_str()) != 0;
                if (on)
                    logger::warn(
                        "[OffscreenFrames] TextureProbe ENABLED -- this maps GPU textures per view "
                        "every 120 frames and WILL cost frames. Diagnostic use only.");
                return on;
            }();

            for (ViewId view : views) {
                const auto renderTarget = backend->ViewRenderTarget(view, presentGeneration);
                if (!renderTarget.validFor(presentGeneration)) continue;
                const uint64_t gen = renderTarget.publishGeneration;

                struct StableOffscreenSnapshot {
                    uint64_t epoch = 0;
                    uint64_t diagFrame = 0;
                    uint64_t lastContentGen = 0;
                    bool haveContent = false;
                    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
                    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
                } snapshot;

                {
                    std::lock_guard lock(g_offscreenMutex);
                    auto it = g_offscreenFrames.find(view);
                    if (it == g_offscreenFrames.end()) continue;

                    const PrismaUI::OffscreenFrameState::Snapshot state{
                        it->second.epoch,
                        it->second.lastContentGen,
                        it->second.haveContent,
                        it->second.texture.Get() != nullptr && it->second.srv.Get() != nullptr,
                    };
                    if (!PrismaUI::OffscreenFrameState::NeedsCopy(state, true, gen)) continue;

                    snapshot.epoch = it->second.epoch;
                    snapshot.diagFrame = it->second.diagFrames++;
                    snapshot.lastContentGen = it->second.lastContentGen;
                    snapshot.haveContent = it->second.haveContent;
                    snapshot.texture = it->second.texture;
                    snapshot.srv = it->second.srv;
                }
                if (snapshot.epoch == 0) continue;

                Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sourceSrv = renderTarget.srv;
                if (!sourceSrv) continue;

                Microsoft::WRL::ComPtr<ID3D11Resource> sourceResource;
                sourceSrv->GetResource(&sourceResource);
                Microsoft::WRL::ComPtr<ID3D11Texture2D> sourceTexture;
                if (!sourceResource || FAILED(sourceResource.As(&sourceTexture))) continue;

                D3D11_TEXTURE2D_DESC sourceDesc{};
                sourceTexture->GetDesc(&sourceDesc);

                uint32_t sourceLeft = 0;
                uint32_t sourceTop = 0;
                uint32_t sourceRight = 0;
                uint32_t sourceBottom = 0;
                if (renderTarget.textureWidth != sourceDesc.Width || renderTarget.textureHeight != sourceDesc.Height ||
                    renderTarget.format != sourceDesc.Format ||
                    !ResolveSourceCoordinate(renderTarget.uvLeft, sourceDesc.Width, sourceLeft) ||
                    !ResolveSourceCoordinate(renderTarget.uvTop, sourceDesc.Height, sourceTop) ||
                    !ResolveSourceCoordinate(renderTarget.uvRight, sourceDesc.Width, sourceRight) ||
                    !ResolveSourceCoordinate(renderTarget.uvBottom, sourceDesc.Height, sourceBottom) ||
                    sourceRight <= sourceLeft || sourceBottom <= sourceTop ||
                    sourceRight - sourceLeft != renderTarget.viewportWidth ||
                    sourceBottom - sourceTop != renderTarget.viewportHeight) {
                    logger::warn("[WebRuntime] rejected invalid offscreen render target for view {}", view);
                    continue;
                }

                Microsoft::WRL::ComPtr<ID3D11Texture2D> destinationTexture = snapshot.texture;
                Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> destinationSrv = snapshot.srv;
                D3D11_TEXTURE2D_DESC current{};
                if (destinationTexture) destinationTexture->GetDesc(&current);
                if (!destinationTexture || !destinationSrv || current.Width != renderTarget.viewportWidth ||
                    current.Height != renderTarget.viewportHeight || current.Format != sourceDesc.Format) {
                    D3D11_TEXTURE2D_DESC desc = sourceDesc;
                    desc.Width = renderTarget.viewportWidth;
                    desc.Height = renderTarget.viewportHeight;
                    desc.Usage = D3D11_USAGE_DEFAULT;
                    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                    desc.CPUAccessFlags = 0;
                    desc.MiscFlags = 0;

                    destinationTexture.Reset();
                    destinationSrv.Reset();
                    if (FAILED(device->CreateTexture2D(&desc, nullptr, &destinationTexture)) ||
                        FAILED(device->CreateShaderResourceView(destinationTexture.Get(), nullptr, &destinationSrv))) {
                        logger::error("[OffscreenFrames] failed to allocate stable offscreen texture for view {}", view);
                        continue;
                    }
                    logger::info("[OffscreenFrames] stable offscreen texture ready: view={} {}x{} format={}", view,
                                 desc.Width, desc.Height, static_cast<int>(desc.Format));
                }

                Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyedMutex;
                const bool haveMutex = SUCCEEDED(sourceTexture.As(&keyedMutex)) && keyedMutex;
                HRESULT acquireHr = S_OK;
                bool mutexAcquired = false;
                if (haveMutex) {
                    acquireHr = keyedMutex->AcquireSync(0, 0);
                    mutexAcquired = acquireHr == S_OK;
                }

                const bool probeNow = s_probeEnabled && ((snapshot.diagFrame < 3) || (snapshot.diagFrame % 120 == 0));
                if (probeNow) {
                    logger::debug(
                        "[DIAG] view={} SOURCE desc {}x{} fmt={} misc=0x{:x} bind=0x{:x} usage={} | "
                        "keyedMutex={} acquireHr=0x{:08x}",
                        view, sourceDesc.Width, sourceDesc.Height, static_cast<int>(sourceDesc.Format),
                        sourceDesc.MiscFlags, sourceDesc.BindFlags, static_cast<int>(sourceDesc.Usage),
                        haveMutex ? "YES" : "NO", static_cast<uint32_t>(acquireHr));
                }

                if (haveMutex && !mutexAcquired) {
                    if (probeNow) logger::debug("[DIAG] view={} source busy -- retaining previous stable frame", view);
                    continue;
                }

                if (probeNow) {
                    const PixelStats src = ProbeTexture(device, context, sourceTexture.Get());
                    logger::debug(
                        "[DIAG] view={} SOURCE(web) pixels: mapped={} nonBlack={}/{} max(RGBA)={},{},{},{} "
                        "centre(RGBA)={},{},{},{}",
                        view, src.mapped, src.nonBlack, src.sampled, src.maxR, src.maxG, src.maxB, src.maxA,
                        src.centreR, src.centreG, src.centreB, src.centreA);
                    if (snapshot.diagFrame == 0) {
                        const auto dumpPath =
                            (PrismaUI::Utils::ThisModuleDir() / ("web_source_view" + std::to_string(view) + ".bmp"))
                                .string();
                        DumpTextureBMP(device, context, sourceTexture.Get(), dumpPath);
                    }
                }

                const D3D11_BOX sourceBox{
                    sourceLeft, sourceTop, 0, sourceRight, sourceBottom, 1,
                };
                context->CopySubresourceRegion(destinationTexture.Get(), 0, 0, 0, 0, sourceTexture.Get(), 0,
                                               &sourceBox);
                if (PrismaUI::OffscreenFrameState::ShouldReleaseKeyedMutex(haveMutex, mutexAcquired)) {
                    keyedMutex->ReleaseSync(0);
                }

                bool published = false;
                {
                    std::lock_guard lock(g_offscreenMutex);
                    auto it = g_offscreenFrames.find(view);
                    if (it != g_offscreenFrames.end() &&
                        PrismaUI::OffscreenFrameState::CanPublish(it->second.epoch, snapshot.epoch)) {
                        it->second.texture = destinationTexture;
                        it->second.srv = destinationSrv;
                        it->second.lastContentGen = gen;
                        it->second.haveContent = true;
                        published = true;
                    }
                }
                if (!published) continue;

                if (probeNow) {
                    const PixelStats dst = ProbeTexture(device, context, destinationTexture.Get());
                    logger::debug(
                        "[DIAG] view={} STABLE(copy) pixels: mapped={} nonBlack={}/{} max(RGBA)={},{},{},{} "
                        "centre(RGBA)={},{},{},{}",
                        view, dst.mapped, dst.nonBlack, dst.sampled, dst.maxR, dst.maxG, dst.maxB, dst.maxA,
                        dst.centreR, dst.centreG, dst.centreB, dst.centreA);
                }
            }
        }
    }

    void Track(ViewId view) {
        std::lock_guard lock(g_offscreenMutex);
        auto [it, inserted] = g_offscreenFrames.try_emplace(view);
        if (inserted) it->second.epoch = NextOffscreenEpochLocked();
    }

    void Forget(ViewId view) {
        std::lock_guard lock(g_offscreenMutex);
        g_offscreenFrames.erase(view);
    }

    bool IsTracked(ViewId view) {
        std::lock_guard lock(g_offscreenMutex);
        return g_offscreenFrames.contains(view);
    }

    void* PeekSRV(ViewId view) {
        std::lock_guard lock(g_offscreenMutex);
        auto it = g_offscreenFrames.find(view);
        return it == g_offscreenFrames.end() ? nullptr : it->second.srv.Get();
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> AcquireSRV(ViewId view) {
        std::lock_guard lock(g_offscreenMutex);
        auto it = g_offscreenFrames.find(view);
        if (it == g_offscreenFrames.end()) {
            return {};
        }
        return it->second.srv;
    }

    void Update(Backend backend, std::uint64_t presentGeneration, ID3D11Device* device, ID3D11DeviceContext* context) {
        UpdateImpl(backend, presentGeneration, device, context);
    }

}
