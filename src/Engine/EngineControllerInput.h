#pragma once

#include "Engine/ControllerMenuKey.h"

#ifdef PRISMAUI_FO4VR
namespace PrismaUI::Engine {
struct ControllerHooks {
    using Sender = void*;
    Sender sender = nullptr;
    void* button = nullptr;
    void* stick = nullptr;
};

inline ControllerHooks ControllerConversionTargets() {
    return {};
}

inline void RegisterMenuInputHandler(RE::MenuControls* mc, RE::BSInputEventUser* sink) {
    bool present = false;
    for (std::uint32_t i = 0; i < mc->handlers.size(); ++i) {
        if (mc->handlers[i] == sink) { present = true; break; }
    }
    if (!present) mc->handlers.push_back(sink);
}

inline void RegisterGameplayInputHandler(RE::PlayerControls* pc, RE::BSInputEventUser* sink) {
    pc->RegisterHandler(static_cast<RE::PlayerInputHandler*>(sink));
}
}
#else
#include "RE/B/BSUIScaleformData.h"
#include "RE/G/GFxConvertHandler.h"

namespace PrismaUI::Engine {
struct ControllerHooks {
    using Sender = RE::BSUIScaleformData::SendEventFunction;
    Sender sender = nullptr;
    RE::GFxConvertHandler::ButtonEventHandler button = nullptr;
    RE::GFxConvertHandler::ThumbstickEventHandler stick = nullptr;
};

inline RE::BSUIScaleformData::SendEventFunction ResolveSendUIScaleformEvent() {
    static REL::Relocation<RE::BSUIScaleformData::SendEventFunction> func{ REL::VariantID{ 922159, 2284983, 2284983 } };
    return func.get();
}

inline ControllerHooks ControllerConversionTargets() {
    auto* controls = RE::MenuControls::GetSingleton();
    if (!controls || !controls->convertHandler) return {};
    const auto version = REX::FModule::GetExecutingModule().GetFileVersion();
    if (version != REL::Version{1, 10, 163, 0} && version != REL::Version{1, 11, 240, 0} && version != REL::Version{1, 10, 984, 0}) return {};
    return {ResolveSendUIScaleformEvent(), controls->convertHandler->GetButtonEventHandler(),
        controls->convertHandler->GetThumbstickEventHandler()};
}

inline void RegisterMenuInputHandler(RE::MenuControls* mc, RE::BSInputEventUser* sink) {
    mc->RegisterHandler(sink);
}

inline void RegisterGameplayInputHandler(RE::PlayerControls* pc, RE::BSInputEventUser* sink) {
    auto* handler = static_cast<RE::PlayerInputHandler*>(sink);
    bool present = false;
    for (std::uint32_t i = 0; i < pc->handlers.size(); ++i) {
        if (pc->handlers[i] == handler) { present = true; break; }
    }
    if (!present) pc->RegisterHandler(handler);
}
}
#endif
