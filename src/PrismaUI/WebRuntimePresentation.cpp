#include "WebRuntime.h"
#include "WebRuntimeInternal.h"
#include "API/API.h"
#include "ActorPreviewSpike.h"
#include "CameraSpike.h"
#include "LocalMapSpike.h"
#include "ModelPreview.h"
#include "PresentProfiler.h"
#include "TextureOverlay.h"
#include <algorithm>
#include <utility>
#include <vector>
namespace PrismaUI::WebRuntime::Internal {
    namespace {
        auto& hp = HP();
        void LogHeldCopyFailure(const char* op, HRESULT hr, DXGI_FORMAT texFormat, DXGI_FORMAT srvFormat,
                                D3D11_SRV_DIMENSION viewDim) {
            static int s_logged = 0;
            if (s_logged >= 8) return;
            ++s_logged;
            logger::warn("[Compositor] held-copy {} failed hr=0x{:08X} texFormat={} srvFormat={} viewDim={}", op,
                         static_cast<uint32_t>(hr), static_cast<uint32_t>(texFormat),
                         static_cast<uint32_t>(srvFormat), static_cast<uint32_t>(viewDim));
        }
        bool CopyTextureForHold(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* sourceSrv,
                                HeldTexturePoolSlot& slot) {
            Microsoft::WRL::ComPtr<ID3D11Resource> resource;
            sourceSrv->GetResource(&resource);
            Microsoft::WRL::ComPtr<ID3D11Texture2D> source;
            if (!resource || FAILED(resource.As(&source)) || !source) return false;
            D3D11_TEXTURE2D_DESC desc{};
            source->GetDesc(&desc);
            if (desc.Width == 0 || desc.Height == 0 || desc.SampleDesc.Count > 1) return false;
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            sourceSrv->GetDesc(&srvDesc);
            if (srvDesc.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D) {
                LogHeldCopyFailure("UnsupportedSrvDim", E_NOTIMPL, desc.Format, srvDesc.Format, srvDesc.ViewDimension);
                return false;
            }
            const auto slotKey = [](const D3D11_TEXTURE2D_DESC& t, const D3D11_SHADER_RESOURCE_VIEW_DESC& s) {
                return PrismaUI::PresentHold::HeldSlotKey{ t.Width, t.Height, static_cast<uint32_t>(t.Format),
                                                          t.MipLevels, t.ArraySize, t.SampleDesc.Count,
                                                          t.SampleDesc.Quality, static_cast<uint32_t>(s.Format),
                                                          static_cast<uint32_t>(s.ViewDimension),
                                                          s.Texture2D.MostDetailedMip, s.Texture2D.MipLevels };
            };
            const PrismaUI::PresentHold::HeldSlotKey want = slotKey(desc, srvDesc);
            bool reusable = false;
            if (slot.texture && slot.srv) {
                D3D11_TEXTURE2D_DESC haveTex{};
                slot.texture->GetDesc(&haveTex);
                D3D11_SHADER_RESOURCE_VIEW_DESC haveSrv{};
                slot.srv->GetDesc(&haveSrv);
                reusable = PrismaUI::PresentHold::HeldSlotReusable(slotKey(haveTex, haveSrv), want);
            }
            Microsoft::WRL::ComPtr<ID3D11Texture2D> replacementTexture;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> replacementSrv;
            ID3D11Texture2D* destination = slot.texture.Get();
            if (!reusable) {
                D3D11_TEXTURE2D_DESC owned = desc;
                owned.Usage = D3D11_USAGE_DEFAULT;
                owned.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                owned.CPUAccessFlags = 0;
                owned.MiscFlags = 0;
                HRESULT hrTex = dev->CreateTexture2D(&owned, nullptr, &replacementTexture);
                if (FAILED(hrTex)) {
                    LogHeldCopyFailure("CreateTexture2D", hrTex, desc.Format, srvDesc.Format, srvDesc.ViewDimension);
                    return false;
                }
                HRESULT hrSrv = dev->CreateShaderResourceView(replacementTexture.Get(), &srvDesc, &replacementSrv);
                if (FAILED(hrSrv)) {
                    LogHeldCopyFailure("CreateShaderResourceView", hrSrv, desc.Format, srvDesc.Format,
                                       srvDesc.ViewDimension);
                    return false;
                }
                destination = replacementTexture.Get();
            }
            ctx->CopyResource(destination, source.Get());
            if (!reusable) {
                slot.texture = std::move(replacementTexture);
                slot.srv = std::move(replacementSrv);
            }
            return true;
        }
    }
    HeldPresentState& HP() {
        static HeldPresentState s;
        return s;
    }
    void ClearHeldPresentState() {
        hp.heldViewSources.clear();
        hp.committedViews.clear();
        hp.committedOverlaySignature.clear();
        hp.committedContentGen.clear();
        hp.presentPolicy.ResetDiagnosticObservation();
    }
    bool OverlayListEquals(const std::vector<ModelPreview::Overlay>& a,
                           const std::vector<ModelPreview::Overlay>& b, bool compareSrv) {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i) {
            const auto& x = a[i];
            const auto& y = b[i];
            if ((compareSrv && x.srv != y.srv) || x.destLeft != y.destLeft || x.destTop != y.destTop ||
                x.destRight != y.destRight || x.destBottom != y.destBottom || x.hasClip != y.hasClip ||
                x.clipLeft != y.clipLeft || x.clipTop != y.clipTop || x.clipRight != y.clipRight ||
                x.clipBottom != y.clipBottom || x.sourceWidth != y.sourceWidth ||
                x.sourceHeight != y.sourceHeight ||
                !PrismaUI::PresentHold::OverlayContentClean(x.contentGeneration, y.contentGeneration)) {
                return false;
            }
        }
        return true;
    }
    bool HeldOverlaysCurrent(const std::vector<ModelPreview::Overlay>& live,
                             const std::vector<HeldOverlay>& held) {
        if (live.size() != held.size()) return false;
        for (std::size_t i = 0; i < live.size(); ++i) {
            const auto& x = live[i];
            const auto& y = held[i].overlay;
            if (x.continuityKey != y.continuityKey ||
                !PrismaUI::PresentHold::OverlayContentClean(x.contentGeneration, y.contentGeneration) ||
                x.destLeft != y.destLeft || x.destTop != y.destTop || x.destRight != y.destRight ||
                x.destBottom != y.destBottom || x.hasClip != y.hasClip || x.clipLeft != y.clipLeft ||
                x.clipTop != y.clipTop || x.clipRight != y.clipRight || x.clipBottom != y.clipBottom ||
                x.sourceWidth != y.sourceWidth || x.sourceHeight != y.sourceHeight) {
                return false;
            }
        }
        return true;
    }
    bool CaptureHeldViewSource(ID3D11Device* dev, ID3D11DeviceContext* ctx, WebRuntime::ViewId view,
                               const PrismaUI::Web::RenderTargetSnapshot& target,
                               const std::vector<ModelPreview::Overlay>& liveOverlays, uint64_t deviceEpoch) {
        if (!dev || !ctx || !target.srv) return false;
        auto& entry = hp.heldViewSources[view];
        const bool firstSight = entry.snapshot.srv == nullptr;
        if (!CopyTextureForHold(dev, ctx, target.srv.Get(), entry.spareWeb)) {
            if (firstSight) hp.heldViewSources.erase(view);
            return false;
        }
        std::map<uint64_t, HeldOverlay> fresh;
        for (const auto& overlay : liveOverlays) {
            if (!overlay.srv) continue;
            HeldOverlay ho;
            if (!CopyTextureForHold(dev, ctx, overlay.srv, ho.texture)) {
                if (firstSight) hp.heldViewSources.erase(view);
                return false;
            }
            ho.continuityKey = overlay.continuityKey;
            ho.overlay = overlay;
            ho.overlay.srv = ho.texture.srv.Get();
            fresh.insert_or_assign(overlay.continuityKey, std::move(ho));
        }
        std::vector<HeldOverlay> newOverlays;
        newOverlays.reserve(entry.overlays.size() + fresh.size());
        for (auto& prev : entry.overlays) {
            const auto f = fresh.find(prev.continuityKey);
            if (f != fresh.end()) {
                newOverlays.push_back(std::move(f->second));
                fresh.erase(f);
            } else {
                newOverlays.push_back(std::move(prev));
            }
        }
        for (const auto& overlay : liveOverlays) {
            const auto f = fresh.find(overlay.continuityKey);
            if (f == fresh.end()) continue;
            newOverlays.push_back(std::move(f->second));
            fresh.erase(f);
        }
        std::swap(entry.web, entry.spareWeb);
        entry.snapshot = target;
        entry.snapshot.srv = entry.web.srv;
        entry.capturedContentGeneration = target.contentGeneration;
        entry.deviceEpoch = deviceEpoch;
        entry.overlays = std::move(newOverlays);
        return true;
    }
    void EraseRemovedOverlays(const std::vector<uint64_t>& removedOverlayKeys, WebCompositor& compositor) {
        if (removedOverlayKeys.empty()) return;
        for (auto& [view, held] : hp.heldViewSources) {
            (void)view;
            std::erase_if(held.overlays, [&](const HeldOverlay& h) {
                return std::find(removedOverlayKeys.begin(), removedOverlayKeys.end(), h.continuityKey) !=
                       removedOverlayKeys.end();
            });
        }
        static std::vector<uint64_t> committedOverlayKeys;
        committedOverlayKeys.clear();
        committedOverlayKeys.reserve(hp.committedOverlaySignature.size());
        for (const auto& o : hp.committedOverlaySignature) committedOverlayKeys.push_back(o.continuityKey);
        if (PrismaUI::PresentHold::CommittedContainsRemoved(committedOverlayKeys, removedOverlayKeys)) {
            compositor.InvalidateCommittedFrame();
            hp.committedViews.clear();
            hp.committedOverlaySignature.clear();
            hp.committedContentGen.clear();
        }
    }
    void RefreshAndPruneHeldSources(
        int64_t presentNowMs, const std::vector<std::pair<int, WebRuntime::ViewId>>& orderedNativeViews,
        const std::map<WebRuntime::ViewId, std::vector<ModelPreview::Overlay>>& perViewOverlays,
        const std::vector<std::pair<ViewId, PrismaUI::Web::RenderTargetSnapshot>>& freshTargets,
        PrismaUI::Web::IWebBackend& backend, WebCompositor& compositor, ID3D11Device* dev, ID3D11DeviceContext* ctx) {
        if (ctx != nullptr && !freshTargets.empty()) {
            const bool refreshDue = hp.presentPolicy.ShouldRefreshHeldSources(presentNowMs);
            static const std::vector<ModelPreview::Overlay> noViewOverlays;
            for (const auto& [view, target] : freshTargets) {
                const auto existing = hp.heldViewSources.find(view);
                const bool missing = existing == hp.heldViewSources.end();
                if (!refreshDue && !missing) continue;
                const auto liveOverlaysIt = perViewOverlays.find(view);
                const auto& liveOverlays =
                    liveOverlaysIt == perViewOverlays.end() ? noViewOverlays : liveOverlaysIt->second;
                if (!missing && target.contentGeneration != 0 &&
                    existing->second.capturedContentGeneration == target.contentGeneration &&
                    HeldOverlaysCurrent(liveOverlays, existing->second.overlays)) {
                    continue;
                }
                if (!CaptureHeldViewSource(compositor.DevicePtr(), compositor.ContextPtr(), view, target, liveOverlays,
                                           backend.DeviceEpoch())) {
                    static std::atomic<int64_t> s_lastCaptureFailLogMs{0};
                    if (presentNowMs - s_lastCaptureFailLogMs.load() > 1000) {
                        s_lastCaptureFailLogMs.store(presentNowMs);
                        logger::warn("[Compositor] held-source capture failed for view {} (heldSources={})", view,
                                     hp.heldViewSources.size());
                    }
                }
            }
            if (refreshDue) hp.presentPolicy.OnHeldSourcesRefreshed(presentNowMs);
        }
        for (auto it = hp.heldViewSources.begin(); it != hp.heldViewSources.end();) {
            bool stillVisible = false;
            for (const auto& [order, view] : orderedNativeViews) {
                (void)order;
                if (view == it->first) {
                    stillVisible = true;
                    break;
                }
            }
            it = stillVisible ? std::next(it) : hp.heldViewSources.erase(it);
        }
    }
    void BuildPresentCandidates(
        const std::vector<std::pair<int, WebRuntime::ViewId>>& orderedNativeViews,
        const std::vector<std::pair<ViewId, PrismaUI::Web::RenderTargetSnapshot>>& freshTargets,
        const std::map<WebRuntime::ViewId, std::vector<ModelPreview::Overlay>>& perViewOverlays,
        PrismaUI::Web::IWebBackend& backend,
        std::vector<PrismaUI::PresentHold::ViewSource>& outViewSources,
        std::vector<PrismaUI::PresentHold::ViewContent>& outViewContents, std::vector<uint64_t>& outCandidateIds,
        std::vector<ModelPreview::Overlay>& outEffectiveOverlays) {
        outViewSources.clear();
        outViewContents.clear();
        outCandidateIds.clear();
        outEffectiveOverlays.clear();
        outViewSources.reserve(orderedNativeViews.size());
        outViewContents.reserve(orderedNativeViews.size());
        outCandidateIds.reserve(orderedNativeViews.size());
        for (const auto& [order, view] : orderedNativeViews) {
            (void)order;
            PrismaUI::PresentHold::ViewSource source;
            source.id = static_cast<uint64_t>(view);
            uint64_t freshContentGeneration = 0;
            for (const auto& [freshView, target] : freshTargets) {
                if (freshView == view) {
                    source.hasFresh = true;
                    freshContentGeneration = target.contentGeneration;
                    break;
                }
            }
            const auto held = hp.heldViewSources.find(view);
            source.hasHeld = held != hp.heldViewSources.end() &&
                             PrismaUI::PresentHold::HeldSourceEpochValid(
                                 backend.PresentationStateValid(), held->second.deviceEpoch,
                                 backend.DeviceEpoch());
            outViewSources.push_back(source);
            if (!source.hasFresh && !source.hasHeld) continue;
            if (source.hasFresh) outViewContents.push_back({source.id, true, freshContentGeneration});
            outCandidateIds.push_back(source.id);
            const auto live = perViewOverlays.find(view);
            const bool haveLive = live != perViewOverlays.end();
            static std::vector<PrismaUI::PresentHold::OverlayInput> overlayInputs;
            overlayInputs.clear();
            int overlayOrder = 0;
            if (source.hasHeld) {
                for (const auto& h : held->second.overlays) {
                    bool overlayLive = false;
                    if (haveLive)
                        for (const auto& lo : live->second)
                            if (lo.continuityKey == h.continuityKey) {
                                overlayLive = true;
                                break;
                            }
                    overlayInputs.push_back({h.continuityKey, overlayOrder++, overlayLive, true, false});
                }
            }
            if (haveLive) {
                for (const auto& lo : live->second) {
                    bool inHeld = false;
                    if (source.hasHeld)
                        for (const auto& h : held->second.overlays)
                            if (h.continuityKey == lo.continuityKey) {
                                inHeld = true;
                                break;
                            }
                    if (!inHeld) overlayInputs.push_back({lo.continuityKey, overlayOrder++, true, false, false});
                }
            }
            for (const auto& r : PrismaUI::PresentHold::ReconcileOverlays(overlayInputs)) {
                if (r.action == PrismaUI::PresentHold::OverlayAction::DrawLive) {
                    if (haveLive)
                        for (const auto& lo : live->second)
                            if (lo.continuityKey == r.key) {
                                outEffectiveOverlays.push_back(lo);
                                break;
                            }
                } else if (r.action == PrismaUI::PresentHold::OverlayAction::DrawHeld) {
                    for (const auto& h : held->second.overlays)
                        if (h.continuityKey == r.key) {
                            outEffectiveOverlays.push_back(h.overlay);
                            break;
                        }
                }
            }
        }
    }
    void GatherViewOverlays(ID3D11Device* dev, ID3D11DeviceContext* ctx, PrismaUI::Web::IWebBackend& backend,
                            std::map<WebRuntime::ViewId, std::vector<ModelPreview::Overlay>>& perViewOverlays) {
        if (!dev || !ctx) return;
        static std::vector<ModelPreview::Overlay> overlays;
        static std::vector<ModelPreview::ViewId> activeViews;
        static std::vector<TextureOverlay::ViewId> texViews;
        static std::vector<std::pair<int, WebRuntime::ViewId>> orderedViews;
        overlays.clear();
        activeViews.clear();
        texViews.clear();
        orderedViews.clear();
        perViewOverlays.clear();
        PluginAPI::DiagTickMeshBindings();
        LocalMapSpike::Tick(dev, ctx);
        CameraSpike::Tick(dev, ctx);
        ActorPreviewSpike::Tick();
        PrismaUI::PresentProfiler::BeginStage(PrismaUI::PresentProfiler::Stage::TickCore, dev, ctx);
        ModelPreview::TickCore(dev, ctx);
        PrismaUI::PresentProfiler::EndStage(PrismaUI::PresentProfiler::Stage::TickCore, ctx);
        ModelPreview::GetActiveViews(activeViews);
        orderedViews.reserve(activeViews.size());
        for (const auto view : activeViews) {
            orderedViews.emplace_back(RuntimeViewOrder(view), view);
        }
        std::sort(orderedViews.begin(), orderedViews.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        for (const auto& ordered : orderedViews) {
            const auto view = ordered.second;
            if (!backend.IsViewValid(view) || !IsRuntimeViewVisible(view)) continue;
            const std::size_t before = overlays.size();
            ModelPreview::GetOverlays(view, overlays);
            auto& viewList = perViewOverlays[view];
            viewList.insert(viewList.end(), overlays.begin() + static_cast<std::ptrdiff_t>(before), overlays.end());
        }
        orderedViews.clear();
        TextureOverlay::GetActiveViews(texViews);
        orderedViews.reserve(texViews.size());
        for (const auto view : texViews) {
            orderedViews.emplace_back(RuntimeViewOrder(view), view);
        }
        std::sort(orderedViews.begin(), orderedViews.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        for (const auto& ordered : orderedViews) {
            const auto view = ordered.second;
            if (!backend.IsViewValid(view) || !IsRuntimeViewVisible(view)) continue;
            const std::size_t before = overlays.size();
            TextureOverlay::GetOverlays(view, overlays);
            auto& viewList = perViewOverlays[view];
            viewList.insert(viewList.end(), overlays.begin() + static_cast<std::ptrdiff_t>(before), overlays.end());
        }
    }
}
