#pragma once

#include <cstdint>
#include <string_view>

namespace PrismaUI::KnownMenus
{
    enum class Mechanism : std::uint8_t
    {
        Hide,
        ConditionalClose,
    };

    struct Policy
    {
        std::string_view menu;
        Mechanism        mechanism;
        std::string_view why;
    };

    inline constexpr Policy kKnownMenus[] = {
        { "PauseMenu",          Mechanism::Hide,             "Hidden while a Prisma panel owns Escape; the menu must still open and close underneath." },
        { "WorkshopMenu",       Mechanism::Hide,             "Replaced by a workshop UI; the engine keeps the workshop session alive for placement and scrap." },
        { "PromptMenu",         Mechanism::Hide,             "Drawn behind a workshop replacement; the vanilla prompt still drives the interaction." },
        { "ButtonBarMenu",      Mechanism::Hide,             "A separate menu from the one it annotates; it must not draw behind a replacement." },
        { "PipboyMenu",         Mechanism::Hide,             "Hidden while a Prisma pipboy draws the device; the native menu keeps lifecycle and input routing." },
        { "TerminalMenu",       Mechanism::Hide,             "Hidden while a Prisma terminal draws the screen; execution reuses the live menu state, including terminalRunResultsCallback." },
        { "FavoritesMenu",      Mechanism::Hide,             "Replaced by a Prisma wheel on the same hotkey; hidden and re-hidden on every reopen." },
        { "ContainerMenu",      Mechanism::ConditionalClose, "Force-closed while a Prisma inventory or quick-loot owns looting; the vanilla modal must not coexist." },
        { "ExamineConfirmMenu", Mechanism::ConditionalClose, "A modal confirm that must not stack over an active Prisma craft or scrap flow." },
    };

    [[nodiscard]] inline const Policy* Find(std::string_view a_menu) noexcept
    {
        for (const auto& policy : kKnownMenus) {
            if (policy.menu == a_menu) {
                return &policy;
            }
        }
        return nullptr;
    }

    [[nodiscard]] inline const char* MechanismName(Mechanism a_mechanism) noexcept
    {
        return a_mechanism == Mechanism::Hide ? "Hide" : "ConditionalClose";
    }
}
