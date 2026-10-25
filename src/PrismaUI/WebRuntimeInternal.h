#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "IWebBackend.h"
#include "InputRegionPolicy.h"
#include "ModelPreview.h"
#include "PresentHoldPolicy.h"
#include "RenderTargetSnapshot.h"
#include "WebCompositor.h"
#include "WebRuntime.h"
namespace PrismaUI::WebRuntime::Internal {
    struct DomReadyRegistration {
        WebRuntime::ViewId view = 0;
        std::function<void(WebRuntime::ViewId)> callback;
    };
    struct ResultRegistration {
        WebRuntime::ViewId view = 0;
        std::function<void(std::string)> callback;
    };
    struct ListenerRegistration {
        WebRuntime::ViewId view = 0;
        std::string name;
        std::function<void(std::string)> callback;
    };
    struct ConsoleRegistration {
        WebRuntime::ViewId view = 0;
        std::function<void(int, std::string, std::string, int)> callback;
    };
    struct ViewPresentationState {
        bool visible = false;
        int order = 0;
    };
    struct RuntimeState {
        PrismaUI::Web::IWebBackend* backend = nullptr;
        std::atomic<bool> backendReady{false};
        std::atomic<bool> active{false};
        std::atomic<bool> initFailed{false};
        std::atomic<bool> startupAccepted{false};
        WebCompositor compositor;
        std::atomic<bool> compositorReady{false};
        std::atomic<HWND> inputHwnd{nullptr};
        std::atomic<ID3D11Device*> device{nullptr};
        std::atomic<ID3D11DeviceContext*> context{nullptr};
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediateContext;
        std::atomic<int> bbW{0}, bbH{0};
        std::atomic<int> clientW{0}, clientH{0};
        std::mutex cbMutex;
        std::uint64_t nextToken = 1;
        std::map<std::uint64_t, DomReadyRegistration> domReadyByToken;
        std::map<std::uint64_t, ResultRegistration> resultByToken;
        std::map<std::uint64_t, ListenerRegistration> listenerByToken;
        std::map<std::uint64_t, ConsoleRegistration> consoleByToken;
        std::set<std::uint64_t> failedDomReadyTokens;
        std::atomic<WebRuntime::ViewId> focusedView{0};
        std::atomic<WebRuntime::ViewId> activationRestoreView{0};
        std::atomic<bool> focusPauseGame{false};
        std::atomic<bool> focusDisableFocusMenu{false};
        std::atomic<WebRuntime::ViewId> inputTargetView{0};
        std::atomic<bool> dockVisible{false};
        std::atomic<WebRuntime::ViewId> bootView{0};
        std::atomic<WebRuntime::ViewId> dockView{0};
        std::atomic<WebRuntime::ViewId> localInspectorOwner{0};
        std::atomic<WebRuntime::ViewId> localInspectorPending{0};
        std::atomic<WebRuntime::ViewId> inspectorTeardownPending{0};
        std::atomic<int64_t> inspectorTeardownRetryAt{0};
        std::atomic<WebRuntime::ViewId> inspectorMouseTarget{0};
        std::atomic<WebRuntime::ViewId> inspectorPointerCaptureTarget{0};
        std::atomic<int> inspectorPointerOriginX{0};
        std::atomic<int> inspectorPointerOriginY{0};
        std::atomic<bool> vanillaCursorHiddenByPrisma{false};
        std::mutex viewPresentationMutex;
        std::map<WebRuntime::ViewId, ViewPresentationState> viewPresentation;
        std::mutex offscreenSizeMutex;
        std::map<WebRuntime::ViewId, std::pair<int, int>> offscreenSizeMap;
        std::mutex roleMutex;
        std::map<WebRuntime::ViewId, std::uint32_t> viewRoles;
        std::set<WebRuntime::ViewId> roleWarnedViews;
        std::atomic<bool> focusSession{false};
        std::atomic<int64_t> lastPresentMs{0};
        std::atomic<int64_t> recoveryStartMs{0};
        std::atomic<std::uint64_t> presentCount{0};
        std::atomic<int64_t> lastCompositeMs{0};
        std::atomic<int64_t> lastSuccessfulPresentationMs{0};
        std::atomic<int64_t> focusEpisodeStartMs{
            0};
        std::atomic<std::uint32_t> focusGeneration{
            0};
        std::atomic<std::uint64_t> compositeCount{0};
        std::atomic<bool> srvPresent{false};
        std::atomic<std::uint64_t> lastPaintCount{0};
        std::atomic<int64_t> lastPaintChangeMs{0};
        std::atomic<void*> lastSwapChain{nullptr};
        std::atomic<int64_t> contextBusySinceMs{
            0};
        std::atomic<unsigned> contextDropStreak{0};
        std::atomic<std::uint64_t> contextDropTotal{0};
        std::mutex inputRegionMutex;
        InputRegionPolicy::State inputRegionState;
        InputRegionPolicy::State activationRestoreInputState;
        std::mutex escapeOwnerMutex;
        std::set<WebRuntime::ViewId> escapeOwners;
        std::mutex loadMutex;
        std::atomic<bool> watchdogRunning{false};
        std::atomic<bool> inputInstallPosted{false};
    };
    RuntimeState& RT();
    struct HeldTexturePoolSlot {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    };
    struct HeldOverlay {
        std::uint64_t continuityKey = 0;
        ModelPreview::Overlay overlay;
        HeldTexturePoolSlot texture;
    };
    struct HeldViewSource {
        PrismaUI::Web::RenderTargetSnapshot snapshot;
        HeldTexturePoolSlot web;
        HeldTexturePoolSlot spareWeb;
        std::vector<HeldOverlay> overlays;
        std::uint64_t capturedContentGeneration = 0;
        std::uint64_t deviceEpoch = 0;
    };
    struct HeldPresentState {
        PrismaUI::PresentHold::Policy presentPolicy;
        std::map<WebRuntime::ViewId, HeldViewSource> heldViewSources;
        std::vector<ModelPreview::Overlay> committedOverlaySignature;
        std::map<std::uint64_t, std::uint64_t> committedContentGen;
        std::vector<std::uint64_t> committedViews;
    };
    HeldPresentState& HP();
    std::int64_t HealthNowMs();
    void ClientToBackbuffer(int& x, int& y);
    bool IsRuntimeViewVisible(WebRuntime::ViewId view);
    int RuntimeViewOrder(WebRuntime::ViewId view);
    std::vector<std::pair<int, WebRuntime::ViewId>> OrderedVisibleOnscreenViews();
    bool HasVisibleOnscreenView();
    bool ViewOwnsEscape(WebRuntime::ViewId view);
    void WarnIfRoleUndeclared(WebRuntime::ViewId view);
    void ForgetViewShell(WebRuntime::ViewId view);
    bool SetVanillaCursorVisible(bool visible);
    void SyncVanillaCursorVisibility();
    void DomReadyTramp(PrismaUI::Web::ViewId view, std::uint64_t token);
    void ResultTramp(std::string json, std::uint64_t token);
    void ListenerTramp(std::string arg, std::uint64_t token);
    void ConsoleTramp(int level, std::string message, std::string source, int line, std::uint64_t token);
    bool DoLoadAndInit();
    void EnsureSystemViews();
    bool FocusImpl(ViewId view, bool pauseGame, bool disableFocusMenu, InputRegionPolicy::CaptureMode mode);
    bool ScheduleFocusRelease(ViewId focused);
    void TickPauseMaintenance();
    void ClearHeldPresentState();
    bool OverlayListEquals(const std::vector<ModelPreview::Overlay>& a, const std::vector<ModelPreview::Overlay>& b,
                           bool compareSrv = true);
    bool HeldOverlaysCurrent(const std::vector<ModelPreview::Overlay>& live, const std::vector<HeldOverlay>& held);
    bool CaptureHeldViewSource(ID3D11Device* dev, ID3D11DeviceContext* ctx, WebRuntime::ViewId view,
                               const PrismaUI::Web::RenderTargetSnapshot& target,
                               const std::vector<ModelPreview::Overlay>& liveOverlays, std::uint64_t deviceEpoch);
    void EraseRemovedOverlays(const std::vector<std::uint64_t>& removedOverlayKeys, WebCompositor& compositor);
    void RefreshAndPruneHeldSources(
        std::int64_t presentNowMs, const std::vector<std::pair<int, WebRuntime::ViewId>>& orderedNativeViews,
        const std::map<WebRuntime::ViewId, std::vector<ModelPreview::Overlay>>& perViewOverlays,
        const std::vector<std::pair<ViewId, PrismaUI::Web::RenderTargetSnapshot>>& freshTargets,
        PrismaUI::Web::IWebBackend& backend, WebCompositor& compositor, ID3D11Device* dev, ID3D11DeviceContext* ctx);
    void BuildPresentCandidates(
        const std::vector<std::pair<int, WebRuntime::ViewId>>& orderedNativeViews,
        const std::vector<std::pair<ViewId, PrismaUI::Web::RenderTargetSnapshot>>& freshTargets,
        const std::map<WebRuntime::ViewId, std::vector<ModelPreview::Overlay>>& perViewOverlays,
        PrismaUI::Web::IWebBackend& backend,
        std::vector<PrismaUI::PresentHold::ViewSource>& outViewSources,
        std::vector<PrismaUI::PresentHold::ViewContent>& outViewContents, std::vector<std::uint64_t>& outCandidateIds,
        std::vector<ModelPreview::Overlay>& outEffectiveOverlays);
    void GatherViewOverlays(ID3D11Device* dev, ID3D11DeviceContext* ctx, PrismaUI::Web::IWebBackend& backend,
                            std::map<WebRuntime::ViewId, std::vector<ModelPreview::Overlay>>& perViewOverlays);
}
