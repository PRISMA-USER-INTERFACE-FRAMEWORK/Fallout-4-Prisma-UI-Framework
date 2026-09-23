#pragma once

#include <cstdint>
#include <string>

namespace PRISMA_UI_API { typedef bool (*MenuSuppressPredicate)(); }

namespace PrismaUI::VanillaUISuppressor
{

    void Install();

    bool SuppressHUDWidget(const char* className, bool suppress);

    bool SuppressVanillaMenu(const char* menuName, bool suppress);

    bool IsMenuSuppressed(const char* menuName);

    bool CloseVanillaMenu(const char* menuName);

    void SuppressVanillaMenuIf(const char* menuName, PRISMA_UI_API::MenuSuppressPredicate predicate);

    void EnableActivateChoiceFilter(bool enable, bool dropDefaultTake);

    void SuppressActivateChoicePerk(std::uint32_t perkFormID, bool suppress);

    bool GetActivateChoiceLabel(std::uint32_t buttonIndex, std::string& outLabel);

    bool TriggerActivateChoice(std::uint32_t buttonIndex);
}
