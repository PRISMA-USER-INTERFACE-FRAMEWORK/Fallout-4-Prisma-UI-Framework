#pragma once

#include "Engine/ControllerMenuKey.h"
#ifndef PRISMAUI_FO4VR
#include "RE/B/BSUIScaleformData.h"
#include "RE/G/GFxConvertHandler.h"
#endif

namespace PrismaUI::Engine {
struct ControllerHooks {
#ifndef PRISMAUI_FO4VR
    using Sender = RE::BSUIScaleformData::SendEventFunction;
    Sender sender = nullptr;
    RE::GFxConvertHandler::ButtonEventHandler button = nullptr;
    RE::GFxConvertHandler::ThumbstickEventHandler stick = nullptr;
#else
    using Sender = void*;
    Sender sender = nullptr;
    void* button = nullptr;
    void* stick = nullptr;
#endif
};

inline ControllerHooks ControllerConversionTargets() {
#ifdef PRISMAUI_FO4VR
    return {};
#else
    auto* controls = RE::MenuControls::GetSingleton();
    if (!controls || !controls->convertHandler) return {};
    const auto version = REX::FModule::GetExecutingModule().GetFileVersion();
    if (version != REL::Version{1, 10, 163, 0} && version != REL::Version{1, 11, 240, 0}) return {};
    return {RE::BSUIScaleformData::GetSendUIScaleformEvent(), controls->convertHandler->GetButtonEventHandler(),
        controls->convertHandler->GetThumbstickEventHandler()};
#endif
}

}
