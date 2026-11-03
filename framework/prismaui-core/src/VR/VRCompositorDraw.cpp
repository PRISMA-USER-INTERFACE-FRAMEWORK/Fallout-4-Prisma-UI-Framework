#include "PCH.h"

#include "VR/VRCompositorDraw.h"
#include "VR/VRCompositorResources.h"

#include <algorithm>
#include <cmath>

namespace PrismaUI::VRCompositor
{
    using namespace Resources;

    namespace
    {
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
    }

    namespace Draw
    {
        [[nodiscard]] DirectX::VertexPositionColorTexture Vertex(
            const WorldPanelGeometry::Vec3& point,
            float u,
            float v,
            float opacity) noexcept
        {
            return {
                DirectX::XMFLOAT3{point.x, point.y, point.z},
                DirectX::XMFLOAT4{opacity, opacity, opacity, opacity},
                DirectX::XMFLOAT2{u, v}
            };
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
    }
}
