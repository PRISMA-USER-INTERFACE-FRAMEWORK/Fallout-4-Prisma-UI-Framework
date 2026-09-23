#pragma once

#include <cstdint>

#include <RE/Fallout.h>

#include "EngineFunctionBodyScan.h"

namespace PrismaUI::Engine {

    class LocalMapRenderer {
    public:
        static LocalMapRenderer Create() { return LocalMapRenderer(InitRendererFn()(false)); }

        bool  valid() const { return m_renderer != nullptr; }
        void* raw()   const { return m_renderer; }

        void SetInitialPosition(RE::NiPoint3& center)              { SetInitialPosFn()(camera(), &center); }
        void SetExtents(RE::NiPoint3& worldMax, RE::NiPoint3& worldMin) { SetExtentsFn()(camera(), &worldMax, &worldMin); }
        void SetMinFrustum(float halfWidth, float halfHeight)      { SetMinFrustumFn()(camera(), halfWidth, halfHeight); }

        void SetZoom(float zoom)                                   { if (auto* fn = SetZoomFn()) fn(camera(), zoom); }

        RE::NiTexture* Render() { return RenderFn()(m_renderer, true); }

        void Destroy() {
            DestroyRaw(m_renderer);
            m_renderer = nullptr;
        }

        static void DestroyRaw(void* renderer) {
            if (!renderer) return;
            RendererDtorFn()(renderer);
            RE::MemoryManager::GetSingleton().Deallocate(renderer, false);
        }

    private:
        explicit LocalMapRenderer(void* renderer) : m_renderer(renderer) {}

        void* camera() const { return reinterpret_cast<std::uint8_t*>(m_renderer) + kCameraOffset; }

        static constexpr std::uintptr_t kCameraOffset = 0x1A0;

        static REL::Relocation<void* (*)(bool)>& InitRendererFn() {
            static REL::Relocation<void* (*)(bool)> fn{ REL::ID{ 433445, 2224093 } };
            return fn;
        }
        static REL::Relocation<void (*)(void*, RE::NiPoint3*)>& SetInitialPosFn() {
            static REL::Relocation<void (*)(void*, RE::NiPoint3*)> fn{ REL::ID{ 1066266, 2194679 } };
            return fn;
        }
        static REL::Relocation<void (*)(void*, float, float)>& SetMinFrustumFn() {
            static REL::Relocation<void (*)(void*, float, float)> fn{ REL::ID{ 773115, 2194682 } };
            return fn;
        }

        using SetZoomFnPtr = void (*)(void*, float);
        static SetZoomFnPtr SetZoomFn() {
            static SetZoomFnPtr s_fn = [] () -> SetZoomFnPtr {
                const std::uintptr_t anchor = SetMinFrustumFn().address();
                const auto at = FindUniqueBytes(reinterpret_cast<const std::uint8_t*>(anchor),
                                                kLocalMapSetZoomWindow, kLocalMapSetZoomBody,
                                                sizeof(kLocalMapSetZoomBody));
                if (at == kNoMatch) {
                    logger::critical("[EngineLocalMap] LocalMapCamera::SetZoom not found within "
                                     "{:#x} bytes of SetMinFrustum ({:#x}) -- either no match or "
                                     "more than one. Zoom will not be applied on this runtime; "
                                     "nothing is called. Re-derive the body for this build.",
                                     kLocalMapSetZoomWindow, anchor);
                    return nullptr;
                }
                logger::info("[EngineLocalMap] SetZoom derived at {:#x} (SetMinFrustum {:#x} + "
                             "{:#x})", anchor + at, anchor, at);
                return reinterpret_cast<SetZoomFnPtr>(anchor + at);
            }();
            return s_fn;
        }
        static REL::Relocation<void (*)(void*, RE::NiPoint3*, RE::NiPoint3*)>& SetExtentsFn() {
            static REL::Relocation<void (*)(void*, RE::NiPoint3*, RE::NiPoint3*)> fn{ REL::ID{ 600273, 2194681 } };
            return fn;
        }
        static REL::Relocation<RE::NiTexture* (*)(void*, bool)>& RenderFn() {
            static REL::Relocation<RE::NiTexture* (*)(void*, bool)> fn{ REL::ID{ 213532, 2194685 } };
            return fn;
        }
        static REL::Relocation<void (*)(void*)>& RendererDtorFn() {
            static REL::Relocation<void (*)(void*)> fn{ REL::ID{ 1277377, 2194684 } };
            return fn;
        }

        void* m_renderer;
    };

}
