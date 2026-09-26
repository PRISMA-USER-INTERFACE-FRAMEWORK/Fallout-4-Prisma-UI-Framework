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
    namespace
    {
        using Microsoft::WRL::ComPtr;

        struct PanelLayout
        {
            bool valid = false;
            RECT leftEyeRect{};
            RECT rightEyeRect{};
            UINT atlasWidth = 0;
            UINT atlasHeight = 0;
        };

        struct ResolvedTarget
        {
            ComPtr<ID3D11ShaderResourceView> textureOwned;
            ID3D11ShaderResourceView* texture = nullptr;
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            float u0 = 0.0f;
            float v0 = 0.0f;
            float u1 = 1.0f;
            float v1 = 1.0f;

            [[nodiscard]] bool IsValid() const noexcept
            {
                return texture && width > 0 && height > 0;
            }
        };

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

        struct Runtime
        {
            ID3D11Device* device = nullptr;
            ID3D11DeviceContext* context = nullptr;
            std::unique_ptr<DirectX::CommonStates> commonStates;
            std::unique_ptr<DirectX::BasicEffect> effect;
            std::unique_ptr<
                DirectX::PrimitiveBatch<DirectX::VertexPositionColorTexture>>
                batch;
            ComPtr<ID3D11InputLayout> inputLayout;
            std::array<ComPtr<ID3D11DepthStencilState>, 9> depthReadStates{};
            std::array<bool, 9> depthStateFailed{};

            ComPtr<ID3D11ShaderResourceView> reticleTexture;
            bool reticleFailed = false;

            ID3D11Texture2D* submittedTexture = nullptr;
            ComPtr<ID3D11RenderTargetView> submittedRenderTarget;
            D3D11_TEXTURE2D_DESC submittedDescription{};
            PanelLayout panelLayout{};

            std::mutex presentationMutex;
            PresentationThreadPolicy::SerializedTracker<std::thread::id> threadTracker;
        };

        [[nodiscard]] Runtime& Get() noexcept
        {

            static auto* runtime = new Runtime();
            return *runtime;
        }

        [[nodiscard]] float Width(const RECT& rectangle) noexcept
        {
            return static_cast<float>(rectangle.right - rectangle.left);
        }

        [[nodiscard]] float Height(const RECT& rectangle) noexcept
        {
            return static_cast<float>(rectangle.bottom - rectangle.top);
        }

        [[nodiscard]] RECT EyeRect(
            const EyeBounds& bounds,
            UINT atlasWidth,
            UINT atlasHeight) noexcept
        {
            const auto scaleX = static_cast<float>(atlasWidth);
            const auto scaleY = static_cast<float>(atlasHeight);
            return {
                static_cast<LONG>(std::lround(bounds.uMin * scaleX)),
                static_cast<LONG>(std::lround(bounds.vMin * scaleY)),
                static_cast<LONG>(std::lround(bounds.uMax * scaleX)),
                static_cast<LONG>(std::lround(bounds.vMax * scaleY))
            };
        }

        [[nodiscard]] bool FiniteBounds(const EyeBounds& bounds) noexcept
        {
            return bounds.valid &&
                   std::isfinite(bounds.uMin) &&
                   std::isfinite(bounds.vMin) &&
                   std::isfinite(bounds.uMax) &&
                   std::isfinite(bounds.vMax) &&
                   bounds.uMax > bounds.uMin &&
                   bounds.vMax > bounds.vMin &&
                   bounds.uMin >= 0.0f &&
                   bounds.vMin >= 0.0f &&
                   bounds.uMax <= 1.0f &&
                   bounds.vMax <= 1.0f;
        }

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

        [[nodiscard]] bool EnsureRenderer() noexcept
        {
            auto& runtime = Get();
            if (!runtime.device || !runtime.context) {
                return false;
            }
            if (runtime.effect && runtime.batch && runtime.inputLayout &&
                runtime.commonStates) {
                return true;
            }

            try {
                runtime.commonStates =
                    std::make_unique<DirectX::CommonStates>(runtime.device);
                runtime.effect =
                    std::make_unique<DirectX::BasicEffect>(runtime.device);
                runtime.effect->SetTextureEnabled(true);
                runtime.effect->SetVertexColorEnabled(true);
                runtime.effect->SetLightingEnabled(false);

                const void* bytecode = nullptr;
                std::size_t bytecodeLength = 0;
                runtime.effect->GetVertexShaderBytecode(&bytecode, &bytecodeLength);
                if (!bytecode || bytecodeLength == 0) {
                    ReleaseDeviceResources();
                    return false;
                }
                if (FAILED(runtime.device->CreateInputLayout(
                        DirectX::VertexPositionColorTexture::InputElements,
                        DirectX::VertexPositionColorTexture::InputElementCount,
                        bytecode,
                        bytecodeLength,
                        runtime.inputLayout.GetAddressOf()))) {
                    ReleaseDeviceResources();
                    return false;
                }
                runtime.batch = std::make_unique<
                    DirectX::PrimitiveBatch<DirectX::VertexPositionColorTexture>>(
                    runtime.context);
                return true;
            } catch (...) {
                ReleaseDeviceResources();
                return false;
            }
        }

        [[nodiscard]] ID3D11DepthStencilState* DepthReadState(
            D3D11_COMPARISON_FUNC comparison) noexcept
        {
            auto& runtime = Get();
            const auto index = static_cast<std::size_t>(comparison);
            if (!runtime.device ||
                index >= runtime.depthReadStates.size() ||
                comparison < D3D11_COMPARISON_NEVER ||
                comparison > D3D11_COMPARISON_ALWAYS) {
                return nullptr;
            }
            if (runtime.depthReadStates[index]) {
                return runtime.depthReadStates[index].Get();
            }
            if (runtime.depthStateFailed[index]) {
                return nullptr;
            }

            D3D11_DEPTH_STENCIL_DESC description{};
            description.DepthEnable = TRUE;
            description.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
            description.DepthFunc = comparison;
            description.StencilEnable = FALSE;
            if (FAILED(runtime.device->CreateDepthStencilState(
                    &description,
                    runtime.depthReadStates[index].GetAddressOf()))) {
                runtime.depthStateFailed[index] = true;
                return nullptr;
            }
            return runtime.depthReadStates[index].Get();
        }

        [[nodiscard]] DirectX::VertexPositionColorTexture Vertex(
            const WorldPanelGeometry::Vec3& point,
            float u,
            float v,
            float opacity = 1.0f) noexcept
        {
            return {
                DirectX::XMFLOAT3{point.x, point.y, point.z},
                DirectX::XMFLOAT4{opacity, opacity, opacity, opacity},
                DirectX::XMFLOAT2{u, v}
            };
        }

        [[nodiscard]] bool ConfigureCommonPipeline() noexcept
        {
            auto& runtime = Get();
            if (!runtime.context || !runtime.commonStates || !runtime.inputLayout) {
                return false;
            }

            runtime.context->SetPredication(nullptr, FALSE);
            runtime.context->HSSetShader(nullptr, nullptr, 0);
            runtime.context->DSSetShader(nullptr, nullptr, 0);
            runtime.context->GSSetShader(nullptr, nullptr, 0);
            runtime.context->IASetInputLayout(runtime.inputLayout.Get());
            runtime.context->OMSetBlendState(
                runtime.commonStates->AlphaBlend(),
                nullptr,
                0xFFFFFFFFu);
            runtime.context->RSSetState(runtime.commonStates->CullNone());
            auto* sampler = runtime.commonStates->LinearClamp();
            runtime.context->PSSetSamplers(0, 1, &sampler);
            return true;
        }

        [[nodiscard]] bool DrawWorldQuad(
            const ResolvedTarget& target,
            const std::array<DirectX::VertexPositionColorTexture, 4>& vertices,
            const StereoProjection::Snapshot& projection,
            bool useDepth,
            const SceneDepthCapture::FrameDepth* sceneDepth) noexcept
        {
            auto& runtime = Get();
            if (!target.IsValid() ||
                !runtime.panelLayout.valid ||
                !EnsureRenderer() ||
                !ConfigureCommonPipeline()) {
                return false;
            }

            ID3D11DepthStencilState* depthState = runtime.commonStates->DepthNone();
            if (useDepth) {
                if (!sceneDepth || !sceneDepth->IsValid()) {
                    return false;
                }
                depthState = DepthReadState(sceneDepth->comparison);
                if (!depthState) {
                    return false;
                }
            }

            auto* renderTarget = runtime.submittedRenderTarget.Get();
            auto* depthView = useDepth && sceneDepth ? sceneDepth->view.Get() : nullptr;
            runtime.context->OMSetRenderTargets(1, &renderTarget, depthView);

            runtime.effect->SetTexture(target.texture);
            runtime.effect->SetView(DirectX::XMMatrixIdentity());

            const std::array<RECT, 2> eyeRectangles{
                runtime.panelLayout.leftEyeRect,
                runtime.panelLayout.rightEyeRect
            };
            for (std::size_t eye = 0; eye < eyeRectangles.size(); ++eye) {
                const auto& rectangle = eyeRectangles[eye];
                if (rectangle.right <= rectangle.left ||
                    rectangle.bottom <= rectangle.top) {
                    runtime.effect->SetTexture(nullptr);
                    return false;
                }

                D3D11_VIEWPORT viewport{};
                viewport.TopLeftX = static_cast<float>(rectangle.left);
                viewport.TopLeftY = static_cast<float>(rectangle.top);
                viewport.Width = Width(rectangle);
                viewport.Height = Height(rectangle);
                viewport.MinDepth = 0.0f;
                viewport.MaxDepth = 1.0f;
                runtime.context->RSSetViewports(1, &viewport);
                runtime.context->OMSetDepthStencilState(depthState, 0);

                const auto& origin = projection.origin[eye];
                runtime.effect->SetWorld(
                    DirectX::XMMatrixTranslation(-origin.x, -origin.y, -origin.z));
                runtime.effect->SetProjection(
                    DirectX::XMLoadFloat4x4(&projection.composite[eye]));
                runtime.effect->Apply(runtime.context);
                runtime.batch->Begin();
                runtime.batch->DrawQuad(
                    vertices[0],
                    vertices[1],
                    vertices[2],
                    vertices[3]);
                runtime.batch->End();
            }
            runtime.effect->SetTexture(nullptr);
            return true;
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

        [[nodiscard]] ID3D11ShaderResourceView* ReticleTexture() noexcept
        {
            auto& runtime = Get();
            if (runtime.reticleTexture) {
                return runtime.reticleTexture.Get();
            }
            if (runtime.reticleFailed || !runtime.device) {
                return nullptr;
            }

            constexpr UINT kSize = 32;
            constexpr float kCenter = (kSize - 1) * 0.5f;
            std::array<std::uint32_t, kSize * kSize> pixels{};
            for (UINT y = 0; y < kSize; ++y) {
                for (UINT x = 0; x < kSize; ++x) {
                    const auto dx = static_cast<float>(x) - kCenter;
                    const auto dy = static_cast<float>(y) - kCenter;
                    const auto distance = std::sqrt(dx * dx + dy * dy);

                    float alpha = 0.0f;
                    float luminance = 1.0f;
                    if (distance <= 5.0f) {
                        alpha = 1.0f;
                    } else if (distance <= 7.0f) {
                        alpha = 1.0f;
                        luminance = 0.0f;
                    } else if (distance <= 8.5f) {
                        alpha = std::clamp(8.5f - distance, 0.0f, 1.0f);
                        luminance = 0.0f;
                    }
                    const auto channel =
                        static_cast<std::uint32_t>(std::lround(luminance * 255.0f));
                    const auto a = static_cast<std::uint32_t>(std::lround(alpha * 255.0f));
                    pixels[y * kSize + x] =
                        (a << 24) | (channel << 16) | (channel << 8) | channel;
                }
            }

            D3D11_TEXTURE2D_DESC description{};
            description.Width = kSize;
            description.Height = kSize;
            description.MipLevels = 1;
            description.ArraySize = 1;
            description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            description.SampleDesc.Count = 1;
            description.Usage = D3D11_USAGE_IMMUTABLE;
            description.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            D3D11_SUBRESOURCE_DATA initial{};
            initial.pSysMem = pixels.data();
            initial.SysMemPitch = kSize * sizeof(std::uint32_t);

            ComPtr<ID3D11Texture2D> texture;
            if (FAILED(runtime.device->CreateTexture2D(
                    &description,
                    &initial,
                    texture.GetAddressOf())) ||
                FAILED(runtime.device->CreateShaderResourceView(
                    texture.Get(),
                    nullptr,
                    runtime.reticleTexture.GetAddressOf()))) {
                runtime.reticleFailed = true;
                runtime.reticleTexture.Reset();
                logger::error("PrismaUI VR could not create its world-panel reticle texture");
                return nullptr;
            }
            return runtime.reticleTexture.Get();
        }

        [[nodiscard]] bool DrawReticle(
            const SpatialPointer::ReticleSnapshot& reticle,
            const StereoProjection::Snapshot& projection,
            bool useDepth,
            const SceneDepthCapture::FrameDepth* sceneDepth) noexcept
        {
            auto* texture = ReticleTexture();
            if (!reticle.visible ||
                !texture ||
                reticle.physicalWidth <= 0.0f ||
                reticle.physicalHeight <= 0.0f) {
                return false;
            }

            const auto cross = [](const auto& left, const auto& right) {
                return WorldPanelGeometry::Vec3{
                    left.y * right.z - left.z * right.y,
                    left.z * right.x - left.x * right.z,
                    left.x * right.y - left.y * right.x
                };
            };
            const auto normal = cross(reticle.right, reticle.up);
            const auto halfRight = reticle.physicalWidth * 0.5f;
            const auto halfUp = reticle.physicalHeight * 0.5f;

            const auto point = [&](float right, float up) {
                return WorldPanelGeometry::Vec3{
                    reticle.center.x + reticle.right.x * right +
                        reticle.up.x * up + normal.x * 0.02f,
                    reticle.center.y + reticle.right.y * right +
                        reticle.up.y * up + normal.y * 0.02f,
                    reticle.center.z + reticle.right.z * right +
                        reticle.up.z * up + normal.z * 0.02f
                };
            };

            const std::array<DirectX::VertexPositionColorTexture, 4> vertices{
                Vertex(point(-halfRight, halfUp), 0.0f, 0.0f),
                Vertex(point(halfRight, halfUp), 1.0f, 0.0f),
                Vertex(point(halfRight, -halfUp), 1.0f, 1.0f),
                Vertex(point(-halfRight, -halfUp), 0.0f, 1.0f)
            };

            ResolvedTarget target;
            target.texture = texture;
            target.width = 32;
            target.height = 32;
            return DrawWorldQuad(target, vertices, projection, useDepth, sceneDepth);
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

    void ReleaseDeviceResources() noexcept
    {
        auto& runtime = Get();
        runtime.batch.reset();
        runtime.effect.reset();
        runtime.commonStates.reset();
        runtime.inputLayout.Reset();
        runtime.reticleTexture.Reset();
        runtime.reticleFailed = false;
        for (auto& state : runtime.depthReadStates) {
            state.Reset();
        }
        runtime.depthStateFailed.fill(false);
        runtime.submittedRenderTarget.Reset();
        runtime.submittedTexture = nullptr;
        runtime.submittedDescription = {};
        runtime.panelLayout = {};
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
