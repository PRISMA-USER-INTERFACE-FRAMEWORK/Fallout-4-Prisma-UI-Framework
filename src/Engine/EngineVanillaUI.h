#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <REL/ID.h>
#include <REL/Relocation.h>
#include <RE/IDs_VTABLE.h>

namespace PrismaUI::Engine {

    inline constexpr std::size_t kHUDCanBeVisibleSlot = 7;
    inline constexpr std::size_t kActivateChoiceUpdatePickRefSlot = 1;
    inline constexpr std::size_t kActivateChoicePopulateDataSlot = 2;

    inline std::uintptr_t ResolveHUDWidgetVtable(std::string_view name) {
        const REL::ID* id = nullptr;

        if (name == "HUDQuestVaultBoy")                  id = &RE::VTABLE::HUDQuestVaultBoy[0];
        else if (name == "HUDObjectiveUpdates")          id = &RE::VTABLE::HUDObjectiveUpdates[0];
        else if (name == "HUDQuestUpdates")              id = &RE::VTABLE::HUDQuestUpdates[0];
        else if (name == "HUDTutorialText")              id = &RE::VTABLE::HUDTutorialText[0];
        else if (name == "HUDExperienceMeter")           id = &RE::VTABLE::HUDExperienceMeter[0];
        else if (name == "HUDMessages")                  id = &RE::VTABLE::HUDMessages[0];
        else if (name == "HUDEnemyHealthMeter")          id = &RE::VTABLE::HUDEnemyHealthMeter[0];
        else if (name == "HUDStealthMeter")              id = &RE::VTABLE::HUDStealthMeter[0];
        else if (name == "FlashVaultBoyCondition")       id = &RE::VTABLE::FlashVaultBoyCondition[0];
        else if (name == "ExplosiveIndicators")          id = &RE::VTABLE::ExplosiveIndicators[0];
        else if (name == "DirectionalHitIndicators")     id = &RE::VTABLE::DirectionalHitIndicators[0];
        else if (name == "HUDCrosshair")                 id = &RE::VTABLE::HUDCrosshair[0];
        else if (name == "HUDRollover")                  id = &RE::VTABLE::HUDRollover[0];
        else if (name == "FlashHitIndicator")            id = &RE::VTABLE::FlashHitIndicator[0];
        else if (name == "HUDQuickContainer")            id = &RE::VTABLE::HUDQuickContainer[0];
        else if (name == "HUDRadiationMeter")            id = &RE::VTABLE::HUDRadiationMeter[0];
        else if (name == "HUDLocationText")              id = &RE::VTABLE::HUDLocationText[0];
        else if (name == "HUDPlayerHealthMeter")         id = &RE::VTABLE::HUDPlayerHealthMeter[0];
        else if (name == "HUDCompass")                   id = &RE::VTABLE::HUDCompass[0];
        else if (name == "HUDSubtitleText")              id = &RE::VTABLE::HUDSubtitleText[0];
        else if (name == "HUDPerkVaultBoy")              id = &RE::VTABLE::HUDPerkVaultBoy[0];
        else if (name == "HUDCriticalMeter")             id = &RE::VTABLE::HUDCriticalMeter[0];
        else if (name == "HUDFlashLightWidget")          id = &RE::VTABLE::HUDFlashLightWidget[0];
        else if (name == "HUDActionPointMeter")          id = &RE::VTABLE::HUDActionPointMeter[0];
        else if (name == "HUDActiveEffectsDisplay")      id = &RE::VTABLE::HUDActiveEffectsDisplay[0];
        else if (name == "HUDAmmoCounter")               id = &RE::VTABLE::HUDAmmoCounter[0];
        else if (name == "HUDExplosiveAmmoCounter")      id = &RE::VTABLE::HUDExplosiveAmmoCounter[0];
        else if (name == "HUDPowerArmorLowBatterWarningText") id = &RE::VTABLE::HUDPowerArmorLowBatterWarningText[0];
        else if (name == "HUDFatigueWarning")            id = &RE::VTABLE::HUDFatigueWarning[0];
        else if (name == "HUDFloatingQuestMarkers")      id = &RE::VTABLE::HUDFloatingQuestMarkers[0];

        return id ? REL::Relocation<std::uintptr_t>{ *id }.address() : 0;
    }

    inline std::uintptr_t ResolveActivateChoiceVtable() {
        return REL::Relocation<std::uintptr_t>{ RE::VTABLE::ActivateChoiceListener[0] }.address();
    }

}
