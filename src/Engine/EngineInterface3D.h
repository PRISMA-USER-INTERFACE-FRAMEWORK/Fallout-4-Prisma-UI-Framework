#pragma once

#include <cstdint>

#include <RE/Fallout.h>

#include "RE/I/Interface3D.h"

namespace PrismaUI::Engine {

    struct RendererArray {
        RE::Interface3D::Renderer** data;
        std::uint64_t               capacity;
        std::uint32_t               size;
    };

    template <class F>
    void ForEachInterface3DRenderer(F&& fn) {
#if defined(PRISMAUI_FO4VR)
        (void)fn;
#else
        if (!REX::FModule::IsRuntimeOG() && !REX::FModule::IsRuntimeAE()) return;
        REL::Relocation<RendererArray*> renderers{ REL::ID{ 996993, 0, 4803727 } };
        auto* arr = renderers.get();
        if (!arr || !arr->data) return;
        for (std::uint32_t i = 0; i < arr->size; ++i) {
            if (auto* r = arr->data[i]) fn(r);
        }
#endif
    }

    inline std::uint32_t Interface3DRendererCount() {
#if defined(PRISMAUI_FO4VR)
        return 0;
#else
        if (!REX::FModule::IsRuntimeOG() && !REX::FModule::IsRuntimeAE()) return 0;
        REL::Relocation<RendererArray*> renderers{ REL::ID{ 996993, 0, 4803727 } };
        auto* arr = renderers.get();
        return arr ? arr->size : 0;
#endif
    }

    inline constexpr const char* kActorPreviewRendererName = "PrismaActorPreview";

    inline void* GetOrCreateActorPreviewRenderer(float a_fov = 45.0f) {
#if defined(PRISMAUI_FO4VR)
        (void)a_fov;
        return nullptr;
#else
        if (!REX::FModule::IsRuntimeOG() && !REX::FModule::IsRuntimeAE()) return nullptr;
        const RE::BSFixedString name{ kActorPreviewRendererName };
        if (auto* existing = RE::Interface3D::Renderer::GetByName(name)) return existing;

        return RE::Interface3D::Renderer::Create(
            name, RE::UI_DEPTH_PRIORITY::kStandard3DModel, a_fov, /*alwaysRenderWhenEnabled*/ true);
#endif
    }

    inline bool AttachPlayer3DToRenderer(void* a_renderer) {
#if defined(PRISMAUI_FO4VR)
        (void)a_renderer;
        return false;
#else
        if (!a_renderer) return false;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return false;
        RE::NiAVObject* root = player->Get3D();
        if (!root) return false;
        static_cast<RE::Interface3D::Renderer*>(a_renderer)->MainScreen_SetWorldAttached3D(root);
        return true;
#endif
    }

    inline bool EnableActorPreview(void* a_renderer) {
#if defined(PRISMAUI_FO4VR)
        (void)a_renderer;
        return false;
#else
        if (!a_renderer) return false;
        static_cast<RE::Interface3D::Renderer*>(a_renderer)->Enable(false);
        return true;
#endif
    }

    inline void StopActorPreview(void* a_renderer) {
#if defined(PRISMAUI_FO4VR)
        (void)a_renderer;
#else
        if (!a_renderer) return;
        auto* r = static_cast<RE::Interface3D::Renderer*>(a_renderer);
        r->MainScreen_SetWorldAttached3D(nullptr);
        r->Disable();
#endif
    }

    inline RE::Interface3D::Renderer* FindRendererOwning(RE::BSGeometry* geom) {
        RE::Interface3D::Renderer* found = nullptr;
        if (!geom) return nullptr;
        ForEachInterface3DRenderer([&](RE::Interface3D::Renderer* r) {
            if (found) return;
            for (const auto& geo : r->displayGeometry) {
                if (geo.get() == geom) { found = r; return; }
            }
        });
        return found;
    }

}
