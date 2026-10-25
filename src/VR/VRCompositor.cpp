#include "PCH.h"

#include "VR/VRCompositor.h"

#include "PrismaUI/WebRuntime.h"
#include "Utils/D3D11StateGuard.h"
#include "VR/PresentationThreadPolicy.h"
#include "VR/SceneDepthCapture.h"
#include "VR/SceneDepthDrawPolicy.h"
#include "VR/SpatialPointer.h"
#include "VR/SpatialPresentation.h"
#include "VR/StereoProjection.h"
#include "VR/VRCompositorDraw.h"
#include "VR/VRCompositorResources.h"
#include "VR/VRViewState.h"
#include "VR/WorldPanelGeometry.h"

#include <DirectXTK/CommonStates.h>
#include <DirectXTK/Effects.h>
#include <DirectXTK/PrimitiveBatch.h>
#include <DirectXTK/VertexTypes.h>

#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace PrismaUI::VRCompositor
{
    using namespace Draw;
    using namespace Resources;

    namespace
    {
        using Microsoft::WRL::ComPtr;

        struct DrawState
        {
            std::shared_ptr<VR::VRView> view;
            ResolvedTarget main;
            PRISMA_UI_VR_API::SpatialUpdateV1 spatial{};
            WorldPanelGeometry::WorldPanelSurface surface{};
            SpatialPointer::ReticleSnapshot reticle{};
            bool world = false;
            bool hasSurface = false;
            bool backendReady = false;
            bool requestsDepth = false;
            bool useDepth = false;
            int order = 0;
        };

        [[nodiscard]] bool EnsureSubmittedTarget(
            ID3D11Texture2D* texture,
            const SubmittedTextureLayout& layout) noexcept
        {
            auto& runtime = Get();
            if (!texture) {
                return false;
            }

            D3D11_TEXTURE2D_DESC description{};
            texture->GetDesc(&description);
            if (description.Width == 0 || description.Height == 0) {
                return false;
            }

            if (runtime.submittedTexture != texture ||
                !runtime.submittedRenderTarget ||
                description.Width != runtime.submittedDescription.Width ||
                description.Height != runtime.submittedDescription.Height ||
                description.Format != runtime.submittedDescription.Format) {
                runtime.submittedRenderTarget.Reset();

                ComPtr<ID3D11Device> device;
                texture->GetDevice(device.GetAddressOf());
                if (!device) {
                    return false;
                }
                if (runtime.device != device.Get()) {
                    ReleaseDeviceResources();
                    runtime.device = device.Get();
                    ComPtr<ID3D11DeviceContext> context;
                    device->GetImmediateContext(context.GetAddressOf());
                    runtime.context = context.Get();
                }

                D3D11_RENDER_TARGET_VIEW_DESC targetDescription{};
                targetDescription.Format = description.Format;
                targetDescription.ViewDimension =
                    description.SampleDesc.Count > 1 ?
                        D3D11_RTV_DIMENSION_TEXTURE2DMS :
                        D3D11_RTV_DIMENSION_TEXTURE2D;
                if (FAILED(runtime.device->CreateRenderTargetView(
                        texture,
                        &targetDescription,
                        runtime.submittedRenderTarget.GetAddressOf()))) {
                    runtime.submittedTexture = nullptr;
                    return false;
                }
                runtime.submittedTexture = texture;
                runtime.submittedDescription = description;
            }

            PanelLayout panel;
            if (layout.stereoPairVerified &&
                FiniteBounds(layout.left) &&
                FiniteBounds(layout.right)) {
                panel.atlasWidth = description.Width;
                panel.atlasHeight = description.Height;
                panel.leftEyeRect =
                    EyeRect(layout.left, panel.atlasWidth, panel.atlasHeight);
                panel.rightEyeRect =
                    EyeRect(layout.right, panel.atlasWidth, panel.atlasHeight);
                panel.valid =
                    panel.leftEyeRect.right > panel.leftEyeRect.left &&
                    panel.leftEyeRect.bottom > panel.leftEyeRect.top &&
                    panel.rightEyeRect.right > panel.rightEyeRect.left &&
                    panel.rightEyeRect.bottom > panel.rightEyeRect.top;
            }
            runtime.panelLayout = panel;
            return panel.valid;
        }

        void NoteResolve(std::uint64_t viewId, const char* stage) noexcept
        {
            static std::mutex m;
            static std::map<std::uint64_t, const char*> last;
            std::lock_guard lock(m);
            const char*& prev = last[viewId];
            if (prev == stage) return;
            prev = stage;
            logger::info("[VRCompositor] view {} -> {}", viewId, stage);
        }

        [[nodiscard]] bool ResolveTarget(DrawState& state) noexcept
        {
            if (!state.view) {
                NoteResolve(0, "no view bound to this draw slot");
                return false;
            }
            const auto viewId = state.view->id;
            if (!WebRuntime::IsValid(viewId)) {
                NoteResolve(viewId, "view handle is no longer valid");
                return false;
            }

            if (!WebRuntime::IsOffscreen(viewId)) {
                WebRuntime::SetViewOffscreen(viewId, true);
            }
            const auto [desiredWidth, desiredHeight] =
                SpatialPresentation::GetDesiredPixelSize(state.view, 0, 0);
            if (desiredWidth > 0 && desiredHeight > 0) {
                int currentWidth = 0;
                int currentHeight = 0;
                if (!WebRuntime::GetOffscreenPageSize(viewId, currentWidth, currentHeight) ||
                    static_cast<std::uint32_t>(currentWidth) != desiredWidth ||
                    static_cast<std::uint32_t>(currentHeight) != desiredHeight) {
                    WebRuntime::SetViewOffscreenSize(
                        viewId,
                        static_cast<int>(desiredWidth),
                        static_cast<int>(desiredHeight));
                }
            }

            state.main.textureOwned = WebRuntime::AcquireViewSRV(viewId);
            if (!state.main.textureOwned) {
                NoteResolve(viewId, "no stable frame yet (not pumped, or CEF has not painted it)");
                return false;
            }

            int pageWidth = 0;
            int pageHeight = 0;
            if (!WebRuntime::GetOffscreenPageSize(viewId, pageWidth, pageHeight) ||
                pageWidth <= 0 || pageHeight <= 0) {
                NoteResolve(viewId, "offscreen page size is unset or zero");
                return false;
            }
            NoteResolve(viewId, "drawable");

            state.main.texture = state.main.textureOwned.Get();
            state.main.width = static_cast<std::uint32_t>(pageWidth);
            state.main.height = static_cast<std::uint32_t>(pageHeight);
            state.main.u0 = 0.0f;
            state.main.v0 = 0.0f;
            state.main.u1 = 1.0f;
            state.main.v1 = 1.0f;
            return true;
        }

        [[nodiscard]] bool BuildWorldVertices(
            DrawState& state,
            const StereoProjection::Snapshot& projection,
            std::array<DirectX::VertexPositionColorTexture, 4>& vertices) noexcept
        {
            WorldPanelGeometry::WorldPanelPlacement placement;
            if (!WorldPanelGeometry::MakePlacement(state.spatial, placement)) {
                return false;
            }
            const WorldPanelGeometry::Vec3 midpoint{
                (projection.origin[0].x + projection.origin[1].x) * 0.5f,
                (projection.origin[0].y + projection.origin[1].y) * 0.5f,
                (projection.origin[0].z + projection.origin[1].z) * 0.5f
            };
            if (!WorldPanelGeometry::ResolveSurface(placement, midpoint, state.surface)) {
                return false;
            }
            const auto& corners = state.surface.corners;
            const auto& main = state.main;
            vertices = {
                Vertex(corners[0], main.u0, main.v0),
                Vertex(corners[1], main.u1, main.v0),
                Vertex(corners[2], main.u1, main.v1),
                Vertex(corners[3], main.u0, main.v1)
            };
            state.hasSurface = true;
            return true;
        }
    }

    void RenderSubmittedTexture(
        ID3D11Texture2D* texture,
        const SubmittedTextureLayout& layout) noexcept
    {
        if (VR::IsShuttingDown() || !texture) {
            return;
        }

        auto& runtime = Get();
        std::unique_lock presentationLock(runtime.presentationMutex, std::try_to_lock);
        if (!presentationLock.owns_lock()) {
            return;
        }

        auto frame = WebRuntime::AdvanceFrame();

        if (!frame.Context()) {
            return;
        }

        const auto observation =
            runtime.threadTracker.Observe(std::this_thread::get_id());
        if (observation.migrated) {
            logger::info(
                "PrismaUI VR observed the stereo submission boundary move threads ({} times)",
                observation.migrationCount);
        }

        if (!EnsureSubmittedTarget(texture, layout) || !EnsureRenderer()) {
            VR::SetRenderBackendOperational(false);
            return;
        }
        VR::SetRenderBackendOperational(true);

        VR::PruneDeadViews();

        std::vector<std::shared_ptr<VR::VRView>> views;
        {
            auto& registry = VR::GetRuntime();
            std::shared_lock lock(registry.viewsMutex);
            views.reserve(registry.views.size());
            for (const auto& [id, view] : registry.views) {
                (void)id;
                if (view) {
                    views.push_back(view);
                }
            }
        }
        if (views.empty()) {
            return;
        }

        std::vector<DrawState> states;
        states.reserve(views.size());
        for (const auto& view : views) {
            VR::SyncFlags(view);
            if (view->destroying.load(std::memory_order_acquire)) {
                continue;
            }

            DrawState state;
            state.view = view;
            if (!SpatialPresentation::BeginFrame(view, state.spatial)) {
                continue;
            }
            state.world =
                state.spatial.presentationMode !=
                PRISMA_UI_VR_API::SpatialPresentationMode::HeadLockedQuad;
            state.requestsDepth =
                state.world &&
                (state.spatial.flags &
                 PRISMA_UI_VR_API::SpatialUpdate_SceneDepthOcclusion) != 0;
            state.order = WebRuntime::GetOrder(view->id);
            states.push_back(std::move(state));
        }
        if (states.empty()) {
            SceneDepthCapture::SetCaptureRequested(false);
            return;
        }

        const auto needsDepth = std::any_of(
            states.begin(),
            states.end(),
            [](const DrawState& state) { return state.requestsDepth; });
        SceneDepthCapture::SetCaptureRequested(needsDepth);
        SceneDepthCapture::SetSubmittedTarget(texture);

        SceneDepthCapture::FrameDepth sceneDepth;
        if (needsDepth) {
            sceneDepth = SceneDepthCapture::AcquireForSubmittedTarget(
                texture,
                runtime.submittedDescription);
        }

        std::erase_if(states, [](DrawState& state) { return !ResolveTarget(state); });
        if (states.empty()) {
            return;
        }

        std::sort(
            states.begin(),
            states.end(),
            [](const DrawState& left, const DrawState& right) {
                return left.order < right.order;
            });

        StereoProjection::Snapshot projection;
        if (!StereoProjection::CaptureSnapshot(projection)) {
            for (auto& state : states) {
                SpatialPresentation::MarkBackendUnavailable(state.view);
            }
            SpatialPointer::HandleBackendUnavailable();
            return;
        }

        ScopedD3D11State stateGuard(runtime.context);

        for (auto& state : states) {
            std::array<DirectX::VertexPositionColorTexture, 4> vertices{};
            if (!BuildWorldVertices(state, projection, vertices)) {
                SpatialPresentation::MarkBackendUnavailable(state.view);
                continue;
            }
            const bool snapshotValid = sceneDepth.IsValid();
            const auto depthMode = PrismaUI::SceneDepthDrawPolicy::Decide(
                state.requestsDepth,
                snapshotValid);
            state.useDepth =
                depthMode == PrismaUI::SceneDepthDrawPolicy::Mode::kOccluded;
            state.backendReady = DrawWorldQuad(
                state.main,
                vertices,
                projection,
                state.useDepth,
                state.useDepth ? &sceneDepth : nullptr);
            auto applied = state.spatial;
            if (!PrismaUI::SceneDepthDrawPolicy::ReportOcclusionApplied(
                    state.requestsDepth,
                    snapshotValid)) {
                applied.flags &=
                    ~PRISMA_UI_VR_API::SpatialUpdate_SceneDepthOcclusion;
            }
            SpatialPresentation::MarkApplied(
                state.view,
                applied,
                state.backendReady,
                state.main.width,
                state.main.height);
        }

        std::vector<SpatialPointer::FrameTarget> targets;
        std::vector<bool> targetUseDepth;
        targets.reserve(states.size());
        targetUseDepth.reserve(states.size());
        for (auto& state : states) {
            if (!state.hasSurface) {
                continue;
            }
            SpatialPointer::FrameTarget target;
            target.view = state.view.get();
            target.surface = &state.surface;
            target.backendReady = state.backendReady;
            target.drawOrder = state.order;
            targets.push_back(target);
            targetUseDepth.push_back(state.useDepth);
        }
        if (targets.empty()) {
            return;
        }
        SpatialPointer::ProcessFrameBatch(std::span(targets));

        for (std::size_t index = 0; index < targets.size(); ++index) {
            const auto& target = targets[index];
            if (!target.reticle.visible) {
                continue;
            }
            const bool useDepth = targetUseDepth[index];
            (void)DrawReticle(
                target.reticle,
                projection,
                useDepth,
                useDepth ? &sceneDepth : nullptr);
        }
    }

    void OnResizeBuffers() noexcept
    {
        auto& runtime = Get();
        std::lock_guard lock(runtime.presentationMutex);
        runtime.submittedRenderTarget.Reset();
        runtime.submittedTexture = nullptr;
        runtime.submittedDescription = {};
        runtime.panelLayout = {};
        SceneDepthCapture::Reset();
    }

    void Shutdown() noexcept
    {
        VR::SetShuttingDown(true);
        VR::SetRenderBackendOperational(false);
        auto& runtime = Get();
        std::lock_guard lock(runtime.presentationMutex);
        ReleaseDeviceResources();
    }

    bool IsOperational() noexcept
    {
        return VR::IsRenderBackendOperational();
    }
}
