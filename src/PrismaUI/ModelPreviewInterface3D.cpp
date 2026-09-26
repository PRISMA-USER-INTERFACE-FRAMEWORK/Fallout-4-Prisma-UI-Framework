#if defined(PRISMA_MODELPREVIEW_INTERFACE3D_PROTOTYPE)

#include "ModelPreviewInterface3D.h"

#include <RE/Fallout.h>

#include <d3d11.h>

namespace PrismaUI::ModelPreviewInterface3D {
namespace {
    RE::Interface3D::Renderer* g_renderer = nullptr;

    constexpr const char* kRendererName = "PrismaModelPreview";
}

ID3D11ShaderResourceView* RenderModelToSRV(RE::NiAVObject* model) {
    if (!model) return nullptr;

    if (!g_renderer) {
        g_renderer = RE::Interface3D::Renderer::GetByName(RE::BSFixedString(kRendererName));
    }
    if (!g_renderer) {
        g_renderer = RE::Interface3D::Renderer::Create(
            RE::BSFixedString(kRendererName),
            RE::UI_DEPTH_PRIORITY::kStandard,
            45.0f,
            true);
    }
    if (!g_renderer) return nullptr;

    g_renderer->Offscreen_Set3D(model);
    g_renderer->Offscreen_Enable3D(true);
    g_renderer->Enable(false);

    auto* rd = RE::BSGraphics::GetRendererData();
    if (!rd) return nullptr;
    const std::int32_t rtIndex = g_renderer->customRenderTarget;
    if (rtIndex < 0 || rtIndex >= 101) return nullptr;
    auto* srView = reinterpret_cast<ID3D11ShaderResourceView*>(rd->renderTargets[rtIndex].srView);
    return srView;
}

void Teardown() {
    if (!g_renderer) return;
    g_renderer->Disable();
    g_renderer = nullptr;
}

}

#endif
