#include "WebRuntime.h"
#include "WebRuntimeInternal.h"
#include <Windows.h>
#include "FreezeDiagnostics.h"
#include "ModelPreview.h"
#include "PresentProfiler.h"
#include "TextureOverlay.h"
#include "Utils/ModulePath.h"
#include "WebInput.h"
#include <algorithm>
#include <map>
#include <utility>
#include <vector>
namespace PrismaUI::WebRuntime {
    namespace {
        auto& st = Internal::RT();
        auto& hp = Internal::HP();
        using namespace Internal;
        bool SkipIdleCompositeEnabled() {
            static const bool s_on = [] {
                const auto ini = PrismaUI::Utils::PluginIniPath().string();
                const bool on = GetPrivateProfileIntA("Perf", "SkipIdleComposite", 0, ini.c_str()) != 0;
                if (on)
                    logger::info(
                        "[WebRuntime] SkipIdleComposite ENABLED -- fullscreen composite is skipped when "
                        "the shell has no visible content and nothing is focused/docked/overlaid");
                return on;
            }();
            return s_on;
        }
    }
    void OnPresent(IDXGISwapChain* swapChain) {
        static const bool s_perfConfigured = [] {
            PrismaUI::PresentProfiler::Configure();
            return true;
        }();
        (void)s_perfConfigured;
        PrismaUI::PresentProfiler::ScopedPresent perfScope;
        st.lastPresentMs.store(HealthNowMs());
        st.presentCount.fetch_add(1, std::memory_order_relaxed);
        st.lastSwapChain.store(swapChain);
        FreezeDiagnostics::SetPresentWindow(st.inputHwnd.load(std::memory_order_acquire));
        FreezeDiagnostics::MarkPresent(FreezeDiagnostics::Stage::Present);
        static std::atomic<bool> s_firstPresentLogged{false};
        if (!s_firstPresentLogged.exchange(true)) {
            logger::info("[D3D] FIRST HookPresent/OnPresent invocation -- swapchain={}",
                         static_cast<const void*>(swapChain));
        }
        TryInstallGameInput();
        auto frame = AdvanceFrame(swapChain);
        struct ProfilerFinalizeGuard {
            PrismaUI::PresentProfiler::ScopedPresent& cpu;
            ID3D11DeviceContext* ctx;
            ~ProfilerFinalizeGuard() noexcept {
                cpu.Finish();
                PrismaUI::PresentProfiler::EndPresent(ctx, st.lastPaintCount.load(), st.contextDropTotal.load());
            }
        } profilerFinalizeGuard{perfScope, frame.Context()};
        auto diagReason = [](int code, const char* why) {
            static int s_last = -999;
            if (code != s_last) {
                s_last = code;
                if (code == 0)
                    logger::info(
                        "[WebRuntime] OnPresent reached the compositor (SRV present) -- see [WebCompositor] for the draw "
                        "result");
                else
                    logger::info("[WebRuntime] OnPresent not drawing: {}", why);
            }
        };
        if (!st.backend) {
            diagReason(1, "Host ABI not resolved");
            return;
        }
        if (!st.active.load()) {
            diagReason(2, "web backend not active yet");
            return;
        }
        if (!st.compositorReady.load()) {
            diagReason(3, "compositor not ready (init failed, or VR build)");
            return;
        }
        const auto orderedNativeViews = OrderedVisibleOnscreenViews();
        const auto inspectorOwner = st.localInspectorOwner.load(std::memory_order_acquire);
        if (inspectorOwner && !IsRuntimeViewVisible(inspectorOwner)) SetInspectorVisibility(inspectorOwner, false);
        if (orderedNativeViews.empty()) {
            (void)hp.presentPolicy.OnPresent(HealthNowMs(), false, false, false, false, false);
            ClearHeldPresentState();
            st.compositor.InvalidateCommittedFrame();
            st.srvPresent.store(false);
            diagReason(4, "no visible native web view");
            return;
        }
        if (swapChain) {
            DXGI_SWAP_CHAIN_DESC d{};
            if (SUCCEEDED(swapChain->GetDesc(&d)) && d.BufferDesc.Width > 0 && d.BufferDesc.Height > 0) {
                const int bw = static_cast<int>(d.BufferDesc.Width);
                const int bh = static_cast<int>(d.BufferDesc.Height);
                int cw = bw, ch = bh;
                if (HWND h = st.inputHwnd.load()) {
                    if (RECT rc{}; GetClientRect(h, &rc) && rc.right > rc.left && rc.bottom > rc.top) {
                        cw = rc.right - rc.left;
                        ch = rc.bottom - rc.top;
                    }
                }
                st.bbW.store(bw);
                st.bbH.store(bh);
                st.clientW.store(cw);
                st.clientH.store(ch);
                static int s_pushedW = 0, s_pushedH = 0;
                if (bw != s_pushedW || bh != s_pushedH) {
                    s_pushedW = bw;
                    s_pushedH = bh;
                    EndInspectorGesture();
                    st.backend->Resize(bw, bh);
                    logger::info(
                        "[WebRuntime] sizes: backbuffer {}x{}, client {}x{} -> shell viewport {}x{} (scale "
                        "{:.3f}x{:.3f})",
                        bw, bh, cw, ch, bw, bh, cw ? static_cast<double>(bw) / cw : 1.0,
                        ch ? static_cast<double>(bh) / ch : 1.0);
                }
            }
        }
        SyncVanillaCursorVisibility();
        int cursorX = 0, cursorY = 0;
        bool drawCursor = WebInput::IsCursorOwned();
        if (drawCursor) {
            cursorX = WebInput::GetLastCursorX();
            cursorY = WebInput::GetLastCursorY();
            if (cursorX < 0 || cursorY < 0) {
                drawCursor = false;
            } else {
                ClientToBackbuffer(cursorX, cursorY);
            }
        }
        const bool idleFrame = SkipIdleCompositeEnabled() && st.focusedView.load() == 0 && !st.dockVisible.load() &&
                               !drawCursor && !ModelPreview::HasPendingWork() && !TextureOverlay::HasPendingWork() &&
                               !HasVisibleOnscreenView();
        if (idleFrame) return;
        std::vector<std::pair<ViewId, PrismaUI::Web::RenderTargetSnapshot>> freshTargets;
        if (frame.Context()) {
            freshTargets.reserve(orderedNativeViews.size());
            for (const auto& [order, view] : orderedNativeViews) {
                (void)order;
                if (!st.backend->IsViewValid(view)) continue;
                auto target = st.backend->ViewRenderTarget(view, frame.PresentGeneration());
                if (target.validForComposition(frame.PresentGeneration())) freshTargets.emplace_back(view, std::move(target));
            }
        }
        PrismaUI::Web::InspectorFrame inspectorFrame;
        const bool inspectorFrameReady = frame.Context() && st.backend->GetInspectorFrame(frame.PresentGeneration(), inspectorFrame);
        static std::map<ViewId, std::vector<ModelPreview::Overlay>> perViewOverlays;
        GatherViewOverlays(frame.Device(), frame.Context(), *st.backend, perViewOverlays);
        const int64_t presentNowMs = HealthNowMs();
        static std::vector<uint64_t> removedOverlayKeys;
        removedOverlayKeys.clear();
        ModelPreview::DrainRemovedOverlayKeys(removedOverlayKeys);
        TextureOverlay::DrainRemovedOverlayKeys(removedOverlayKeys);
        EraseRemovedOverlays(removedOverlayKeys, st.compositor);
        RefreshAndPruneHeldSources(presentNowMs, orderedNativeViews, perViewOverlays, freshTargets, *st.backend,
                                   st.compositor, frame.Device(), frame.Context());
        static std::vector<PrismaUI::PresentHold::ViewSource> viewSources;
        static std::vector<PrismaUI::PresentHold::ViewContent> viewContents;
        static std::vector<uint64_t> candidateIds;
        static std::vector<ModelPreview::Overlay> effectiveOverlays;
        BuildPresentCandidates(orderedNativeViews, freshTargets, perViewOverlays, *st.backend, viewSources,
                               viewContents, candidateIds, effectiveOverlays);
        if (inspectorFrameReady) {
            ModelPreview::Overlay inspectorOverlay;
            inspectorOverlay.srv = inspectorFrame.target.srv.Get();
            inspectorOverlay.sourceWidth = inspectorFrame.target.viewportWidth;
            inspectorOverlay.sourceHeight = inspectorFrame.target.viewportHeight;
            inspectorOverlay.destLeft = inspectorFrame.x;
            inspectorOverlay.destTop = inspectorFrame.y;
            inspectorOverlay.destRight = inspectorFrame.x + static_cast<long>(inspectorFrame.width);
            inspectorOverlay.destBottom = inspectorFrame.y + static_cast<long>(inspectorFrame.height);
            inspectorOverlay.contentGeneration = inspectorFrame.target.contentGeneration;
            inspectorOverlay.continuityKey = (uint64_t(1) << 63) | inspectorFrame.view;
            effectiveOverlays.push_back(inspectorOverlay);
        }
        const auto cls = PrismaUI::PresentHold::Classify(viewSources, hp.committedViews);
        const bool contentChanged = PrismaUI::PresentHold::AnyContentChanged(viewContents, hp.committedContentGen);
        const bool viewSetChanged = candidateIds != hp.committedViews;
        const bool liveObservable = frame.Context() != nullptr;
        const bool overlayChanged = liveObservable && !OverlayListEquals(effectiveOverlays, hp.committedOverlaySignature);
        const bool needsRecompose = contentChanged || viewSetChanged || overlayChanged;
        static std::map<uint64_t, uint64_t> freshContentGenerations;
        freshContentGenerations.clear();
        for (const auto& [view, target] : freshTargets) {
            freshContentGenerations[static_cast<uint64_t>(view)] = target.contentGeneration;
        }
        const bool freshGenerationArrived =
            hp.presentPolicy.ObserveFreshContentGenerations(freshContentGenerations, presentNowMs);
        const bool contentGenerationStalled =
            contentChanged && hp.presentPolicy.ContentGenerationStalled(presentNowMs);
        const bool freshSourceMissing = freshTargets.size() < orderedNativeViews.size();
        const bool haveCommitted = st.compositor.HasCommittedFrame();
        const bool leaseAvailable = frame.Context() != nullptr;
        const auto decision =
            hp.presentPolicy.OnPresent(presentNowMs, true, cls.complete, cls.committedFaithful, haveCommitted, needsRecompose);
        if (decision.action == PrismaUI::PresentHold::Action::Dark) {
            hp.presentPolicy.ObserveStarvation(presentNowMs, false);
            st.srvPresent.store(false);
            if (hp.presentPolicy.Latched(presentNowMs)) {
                diagReason(8, "dark-latched after a failed/skipped present blit; retrying after the latch window");
            } else {
                diagReason(frame.Context() ? 5 : 6,
                           frame.Context() ? "no complete presentable frame yet (visible views lack sources)"
                                           : "present lease unavailable and nothing committed to hold");
            }
            return;
        }
        bool composedFresh = false;
        bool composeFailed = false;
        bool commitSucceeded = false;
        if (decision.action == PrismaUI::PresentHold::Action::ComposeCommit) {
            static std::vector<PrismaUI::Web::RenderTargetSnapshot> layers;
            layers.clear();
            static std::map<uint64_t, uint64_t> pendingContentGen;
            pendingContentGen.clear();
            for (const auto& [order, view] : orderedNativeViews) {
                (void)order;
                const PrismaUI::Web::RenderTargetSnapshot* fresh = nullptr;
                for (const auto& [freshView, target] : freshTargets) {
                    if (freshView == view) {
                        fresh = &target;
                        break;
                    }
                }
                if (fresh) {
                    layers.push_back(*fresh);
                    pendingContentGen[static_cast<uint64_t>(view)] = fresh->contentGeneration;
                    composedFresh = true;
                    continue;
                }
                const auto held = hp.heldViewSources.find(view);
                if (held == hp.heldViewSources.end()) continue;
                layers.push_back(held->second.snapshot);
                pendingContentGen[static_cast<uint64_t>(view)] = held->second.snapshot.contentGeneration;
            }
            const int bbW = st.bbW.load();
            const int bbH = st.bbH.load();
            const auto commitResult =
                layers.empty() || bbW <= 0 || bbH <= 0
                    ? CompositeResult::InvalidArguments
                    : st.compositor.ComposeAndCommitLayers(layers, effectiveOverlays, static_cast<uint32_t>(bbW),
                                                           static_cast<uint32_t>(bbH));
            if (commitResult == CompositeResult::Success) {
                commitSucceeded = true;
                hp.committedViews = candidateIds;
                hp.committedOverlaySignature = effectiveOverlays;
                hp.committedContentGen = pendingContentGen;
                hp.presentPolicy.OnCommitSucceeded();
                if (composedFresh) {
                    st.lastCompositeMs.store(HealthNowMs());
                    FreezeDiagnostics::NoteCompositeSuccess();
                    const uint64_t n = st.compositeCount.fetch_add(1) + 1;
                    if (n == 1) {
                        logger::info(
                            "[Compositor] FIRST SUCCESSFUL COMPOSITOR COMMIT -- complete UI frame composed "
                            "offscreen (the present blit decides display; not a display guarantee)");
                    }
                }
            } else {
                composeFailed = true;
                static std::atomic<int64_t> s_lastComposeFailLogMs{0};
                const int64_t now = HealthNowMs();
                if (now - s_lastComposeFailLogMs.load() > 1000) {
                    s_lastComposeFailLogMs.store(now);
                    logger::warn("[Compositor] offscreen compose failed: {} (commits so far={})",
                                 ToString(commitResult), hp.presentPolicy.Commits());
                }
                composedFresh = false;
                if (hp.presentPolicy.OnComposeFailed(cls.committedFaithful, st.compositor.HasCommittedFrame()) !=
                    PrismaUI::PresentHold::Action::BlitCommitted) {
                    st.srvPresent.store(false);
                    diagReason(9, "compose failed with no faithful committed frame; dark until the next fresh frame");
                    return;
                }
            }
        }
        const auto committedSnapshot = st.compositor.CommittedSnapshot();
        static const std::vector<ModelPreview::Overlay> noBlitOverlays;
        if (frame.Context()) {
            PrismaUI::PresentProfiler::BeginStage(PrismaUI::PresentProfiler::Stage::Draw, frame.Device(),
                                                  frame.Context());
        }
        const auto blitResult =
            st.compositor.Draw(swapChain, committedSnapshot, noBlitOverlays, cursorX, cursorY, drawCursor);
        if (frame.Context()) {
            PrismaUI::PresentProfiler::EndStage(PrismaUI::PresentProfiler::Stage::Draw, frame.Context());
        }
        if (blitResult == CompositeResult::Success) {
            hp.presentPolicy.OnBlitSucceeded();
            const ViewId focused = st.focusedView.load(std::memory_order_acquire);
            if (focused != 0 &&
                std::find(hp.committedViews.begin(), hp.committedViews.end(), static_cast<uint64_t>(focused)) !=
                    hp.committedViews.end()) {
                st.lastSuccessfulPresentationMs.store(HealthNowMs(), std::memory_order_release);
            }
            st.srvPresent.store(true);
            if (composedFresh || commitSucceeded) {
                hp.presentPolicy.ObserveStarvation(presentNowMs, false);
                diagReason(0, nullptr);
            } else {
                const auto reuseInput = PrismaUI::PresentHold::ReuseDiagnosticInput{
                    leaseAvailable,
                    freshSourceMissing,
                    contentGenerationStalled,
                    viewSetChanged,
                    overlayChanged,
                    composeFailed,
                    needsRecompose,
                    cls.committedFaithful,
                    haveCommitted,
                };
                const bool starvationSustained = hp.presentPolicy.ObserveStarvation(
                    presentNowMs, !composeFailed && leaseAvailable && !freshGenerationArrived &&
                                     !viewSetChanged && needsRecompose &&
                                     (freshSourceMissing || contentGenerationStalled) && haveCommitted &&
                                     cls.committedFaithful);
                const auto diagnostic =
                    PrismaUI::PresentHold::ClassifyReuseDiagnostic(reuseInput, starvationSustained);
                static std::atomic<int64_t> s_lastHeldLogMs{0};
                if (presentNowMs - s_lastHeldLogMs.load() > 1000) {
                    s_lastHeldLogMs.store(presentNowMs);
                    logger::info(
                        "[Compositor] committed-frame reuse (reason={}, heldStreak={}, committedViews={}, "
                        "freshTargets={}/{}, contentChanged={}, viewSetChanged={}, overlayChanged={}, "
                        "contentGenerationStalled={}, needsRecompose={}, lease={}, starvationCandidate={})",
                        PrismaUI::PresentHold::ReuseReasonName(diagnostic.reason), hp.presentPolicy.HeldStreak(),
                        hp.committedViews.size(), freshTargets.size(), orderedNativeViews.size(),
                        contentChanged ? 1 : 0, viewSetChanged ? 1 : 0, overlayChanged ? 1 : 0,
                        contentGenerationStalled ? 1 : 0, needsRecompose ? 1 : 0, frame.Context() ? 1 : 0,
                        diagnostic.starvationCandidate ? 1 : 0);
                    if (diagnostic.starvationCandidate || composeFailed) {
                        logger::warn(
                            "[Compositor] committed-frame reuse requires attention (reason={}, heldStreak={}, "
                            "starvationCandidate={})",
                            PrismaUI::PresentHold::ReuseReasonName(diagnostic.reason), hp.presentPolicy.HeldStreak(),
                            diagnostic.starvationCandidate ? 1 : 0);
                    }
                }
            }
        } else if (blitResult == CompositeResult::SafeSkip) {
            hp.presentPolicy.ObserveStarvation(presentNowMs, false);
            hp.presentPolicy.OnBlitSkipped(presentNowMs);
            st.srvPresent.store(false);
            static std::atomic<int64_t> s_lastSkipLogMs{0};
            if (presentNowMs - s_lastSkipLogMs.load() > 1000) {
                s_lastSkipLogMs.store(presentNowMs);
                logger::warn(
                    "[Compositor] SafeSkip: staying off the backbuffer this frame (commits so far={}, "
                    "displayedBefore={} -> latch={})",
                    hp.presentPolicy.Commits(), hp.presentPolicy.HasDisplayed() ? 1 : 0,
                    hp.presentPolicy.Latched(presentNowMs) ? 1 : 0);
            }
            const ViewId focused = st.focusedView.load();
            if (focused != 0) {
                ScheduleFocusRelease(focused);
            }
        } else {
            hp.presentPolicy.ObserveStarvation(presentNowMs, false);
            hp.presentPolicy.OnBlitFailed(presentNowMs);
            st.srvPresent.store(false);
            static std::atomic<int64_t> s_lastFailLogMs{0};
            if (presentNowMs - s_lastFailLogMs.load() > 1000) {
                s_lastFailLogMs.store(presentNowMs);
                logger::warn("[Compositor] present blit did not composite: {} -- dark-latching {}ms (blitFailures={})",
                             ToString(blitResult), PrismaUI::PresentHold::kDarkLatchMs,
                             hp.presentPolicy.BlitFailures());
            }
        }
    }
}
