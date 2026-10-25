#pragma once

#include <cstdint>

#include <RE/Fallout.h>

#include "RE/I/Interface3D.h"

#include "Interface3DOffscreenSlotPolicy.h"

namespace PrismaUI::Engine {

    inline constexpr const char* kActorPreviewRendererName = "PrismaActorPreview";

    struct OffscreenSlotPoll {
        Interface3DOffscreenSlotPolicy::SlotState state = Interface3DOffscreenSlotPolicy::SlotState::UnknownLayout;
        std::int32_t slot = Interface3DOffscreenSlotPolicy::kNoSlot;
    };

    inline RE::Interface3D::Renderer* FindRendererByName(const RE::BSFixedString& a_name) {
        return RE::Interface3D::Renderer::GetByName(a_name);
    }

#if defined(PRISMAUI_FO4VR)
    template <class F>
    void ForEachInterface3DRenderer(F&& fn) {
        (void)fn;
    }

    inline std::uint32_t Interface3DRendererCount() {
        return 0;
    }

    inline void* GetOrCreateActorPreviewRenderer(float a_fov = 45.0f) {
        (void)a_fov;
        return nullptr;
    }

    inline bool AttachPlayer3DToRenderer(void* a_renderer) {
        (void)a_renderer;
        return false;
    }

    inline bool EnableActorPreview(void* a_renderer) {
        (void)a_renderer;
        return false;
    }

    inline void StopActorPreview(void* a_renderer) {
        (void)a_renderer;
    }

    inline OffscreenSlotPoll PollOffscreenSlot(void* a_renderer) {
        (void)a_renderer;
        return {};
    }
#else
    inline RE::BSTArray<RE::Interface3D::Renderer*>* Interface3DRenderers() {
        static REL::Relocation<RE::BSTArray<RE::Interface3D::Renderer*>*> renderers{ REL::VariantID{ 996993, 2696455, 4803746 } };
        return renderers.get();
    }

    template <class F>
    void ForEachInterface3DRenderer(F&& fn) {
        auto* arr = Interface3DRenderers();
        if (!arr) return;
        for (auto* r : *arr) {
            if (r) fn(r);
        }
    }

    inline std::uint32_t Interface3DRendererCount() {
        auto* arr = Interface3DRenderers();
        return arr ? arr->size() : 0;
    }

    inline RE::Interface3D::Renderer* CreateRenderer(const RE::BSFixedString& a_name, RE::UI_DEPTH_PRIORITY a_depth, float a_fov, bool a_alwaysRender) {
        return RE::Interface3D::Renderer::Create(a_name, a_depth, a_fov, a_alwaysRender);
    }

    inline void* GetOrCreateActorPreviewRenderer(float a_fov = 45.0f) {
        if (!REX::FModule::IsRuntimeOG() && !REX::FModule::IsRuntimeAE() && !REX::FModule::IsRuntimeNG()) return nullptr;
        const RE::BSFixedString name{ kActorPreviewRendererName };
        if (auto* existing = FindRendererByName(name)) return existing;

        return CreateRenderer(
            name, RE::UI_DEPTH_PRIORITY::kStandard3DModel, a_fov, /*alwaysRenderWhenEnabled*/ true);
    }

    inline bool AttachPlayer3DToRenderer(void* a_renderer) {
        if (!a_renderer) return false;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return false;
        RE::NiAVObject* root = player->Get3D();
        if (!root) return false;
        static_cast<RE::Interface3D::Renderer*>(a_renderer)->MainScreen_SetWorldAttached3D(root);
        return true;
    }

    inline bool EnableActorPreview(void* a_renderer) {
        if (!a_renderer) return false;
        static_cast<RE::Interface3D::Renderer*>(a_renderer)->Enable(false);
        return true;
    }

    inline void StopActorPreview(void* a_renderer) {
        if (!a_renderer) return;
        auto* r = static_cast<RE::Interface3D::Renderer*>(a_renderer);
        r->MainScreen_SetWorldAttached3D(nullptr);
        r->Disable();
    }

    inline OffscreenSlotPoll PollOffscreenSlot(void* a_renderer) {
        OffscreenSlotPoll poll{};
        if (!a_renderer) return poll;
        const auto layout = Interface3DOffscreenSlotPolicy::FlatRuntimeLayout(
            REX::FModule::IsRuntimeOG(), REX::FModule::IsRuntimeNG());
        poll.slot = static_cast<RE::Interface3D::Renderer*>(a_renderer)->customRenderTarget;
        poll.state = Interface3DOffscreenSlotPolicy::Classify(layout, poll.slot);
        return poll;
    }
#endif

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
