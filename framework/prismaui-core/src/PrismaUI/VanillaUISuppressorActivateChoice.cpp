#include "VanillaUISuppressor.h"
#include "../Engine/EngineVanillaUI.h"
#include "../RuntimeSupportPolicy.h"

#include <REL/Relocation.h>
#include <REX/FModule.h>

#include <array>
#include <atomic>
#include <mutex>
#include <set>
#include <string>
#include <utility>

namespace PrismaUI::VanillaUISuppressor
{
    namespace
    {

        using PopulateDataFn = void (*)(void*, std::uint32_t, void*);
        PopulateDataFn    g_origPopulateData = nullptr;
        std::atomic<bool> g_captureEnabled{ false };

        using UpdatePickRefFn = void (*)(void*, void*);
        UpdatePickRefFn g_origUpdatePickRef = nullptr;

        struct ChoiceSlot { bool valid = false; std::string label; std::uint64_t generation = 0; };

        constexpr std::size_t BTN_COUNT = 4;

        std::mutex                        g_choiceMutex;
        std::array<ChoiceSlot, BTN_COUNT> g_choiceSlots;
        std::atomic<std::uint64_t>        g_choiceGeneration{ 0 };
        std::atomic<void*>                g_lastPickRef{ nullptr };

        void Detour_UpdatePickRef(void* self, void* pickRef)
        {
            if (g_origUpdatePickRef) g_origUpdatePickRef(self, pickRef);
            if (!g_captureEnabled.load()) return;

            void* previous = g_lastPickRef.exchange(pickRef);
            if (previous != pickRef) {
                g_choiceGeneration.fetch_add(1, std::memory_order_acq_rel);
                std::lock_guard lock{ g_choiceMutex };
                for (auto& s : g_choiceSlots) s = {};
            }

            static std::atomic<int> s_count{ 0 };
            const int n = s_count.fetch_add(1);
            if (n < 12 || (n % 250) == 0) {
                logger::info("VanillaUISuppressor: UpdatePickRef call #{} (self={}, pickRef={})",
                             n, self != nullptr, pickRef != nullptr);
            }
        }

        void WarnFilteringUnimplemented(const char* what)
        {
            static std::once_flag once;
            std::call_once(once, [what] {
                logger::warn("VanillaUISuppressor: {} requested, but activate-choice FILTERING is not "
                             "implemented -- EnableActivateChoiceFilter only CAPTURES the choice row for "
                             "GetActivateChoiceLabel/TriggerActivateChoice. No vanilla button is removed.",
                             what);
            });
        }

        void Detour_PopulateData(void* self, std::uint32_t buttonIndex, void* out)
        {
            g_origPopulateData(self, buttonIndex, out);
            (void)self;
            if (!g_captureEnabled.load() || buttonIndex >= g_choiceSlots.size()) return;

            const char* const* labelSlot = reinterpret_cast<const char* const*>(out);
            const char*        label     = labelSlot ? *labelSlot : nullptr;

            {
                static std::mutex                                   s_seenMutex;
                static std::set<std::pair<std::uint32_t, std::string>> s_seen;
                std::string labelStr = (label && label[0]) ? label : "";
                auto        key      = std::make_pair(buttonIndex, labelStr);
                bool        fresh    = false;
                {
                    std::lock_guard lk{ s_seenMutex };
                    fresh = s_seen.insert(key).second;
                }
                if (fresh) {
                    logger::info("VanillaUISuppressor: PopulateData btn={} label='{}'",
                                 buttonIndex, labelStr);
                }
            }

            const std::uint64_t gen = g_choiceGeneration.load(std::memory_order_acquire);
            std::lock_guard lock{ g_choiceMutex };
            if (buttonIndex == 0) {
                for (auto& s : g_choiceSlots) s = {};
            }

            if (label && label[0] != '\0') {
                g_choiceSlots[buttonIndex] = { true, std::string(label), gen };
            } else {
                g_choiceSlots[buttonIndex] = {};
            }
        }
    }

    void EnableActivateChoiceFilter(bool a_enable, bool a_dropDefaultTake)
    {
        if (a_dropDefaultTake) WarnFilteringUnimplemented("dropDefaultTake");

        if (a_enable && g_origPopulateData == nullptr) {
            const auto runtime = REX::FModule::GetExecutingModule().GetFileVersion();
            if (!PrismaUI::RuntimeSupportPolicy::IsSupportedGameVersion(
                    runtime.major(), runtime.minor(), runtime.patch())) {
                logger::warn("VanillaUISuppressor::EnableActivateChoiceFilter: refused -- only Fallout "
                             "4 OG 1.10.163, AE 1.11.137 or later, and NG 1.10.984 are supported");
                return;
            }
            REL::Relocation<std::uintptr_t> vtable{ PrismaUI::Engine::ResolveActivateChoiceVtable() };

            const auto rdataSection = REX::FModule::GetExecutingModule().GetSection(".rdata");
            if (rdataSection.GetSize() == 0 ||
                vtable.address() < rdataSection.GetAddress() ||
                vtable.address() >= rdataSection.GetAddress() + rdataSection.GetSize()) {
                logger::critical("VanillaUISuppressor::EnableActivateChoiceFilter: REFUSING to hook -- "
                                 "resolved vtable address {:#x} (RVA {:#x}) "
                                 "does not fall inside .rdata ({:#x}..{:#x}); named authority is almost certainly wrong.",
                                 vtable.address(), vtable.offset(),
                                 rdataSection.GetAddress(), rdataSection.GetAddress() + rdataSection.GetSize());
                return;
            }

            const std::uintptr_t slotAddr = vtable.address() + sizeof(void*) * PrismaUI::Engine::kActivateChoicePopulateDataSlot;

            const auto textSection = REX::FModule::GetExecutingModule().GetSection(".text");
            const std::uintptr_t currentValue = *reinterpret_cast<std::uintptr_t*>(slotAddr);
            if (currentValue < textSection.GetAddress() ||
                currentValue >= textSection.GetAddress() + textSection.GetSize()) {
                logger::critical("VanillaUISuppressor::EnableActivateChoiceFilter: REFUSING to hook -- "
                                 "ActivateChoiceListener vtable slot {} value {:#x} is not inside .text "
                                 "({:#x}..{:#x}); named authority is almost certainly wrong.",
                                 PrismaUI::Engine::kActivateChoicePopulateDataSlot, currentValue,
                                 textSection.GetAddress(), textSection.GetAddress() + textSection.GetSize());
                return;
            }

            {
                const std::uintptr_t moduleBase = REX::FModule::GetExecutingModule().GetBaseAddress();
                const std::uintptr_t slot1Val   = *reinterpret_cast<std::uintptr_t*>(
                    vtable.address() + sizeof(void*) * PrismaUI::Engine::kActivateChoiceUpdatePickRefSlot);
                logger::info("VanillaUISuppressor: [RVA CHECK] ActivateChoiceListener (resolved {:#x}) slot1 -> RVA {:#x}, "
                             "slot2 -> RVA {:#x}",
                             vtable.offset(),
                             slot1Val     - moduleBase,
                             currentValue - moduleBase);
            }

            const std::uintptr_t original = vtable.write_vfunc(PrismaUI::Engine::kActivateChoicePopulateDataSlot, &Detour_PopulateData);
            g_origPopulateData = reinterpret_cast<PopulateDataFn>(original);
            logger::info("VanillaUISuppressor: installed ActivateChoiceListener::PopulateData detour "
                         "(slot {})", PrismaUI::Engine::kActivateChoicePopulateDataSlot);

            const std::uintptr_t origUpr = vtable.write_vfunc(PrismaUI::Engine::kActivateChoiceUpdatePickRefSlot, &Detour_UpdatePickRef);
            g_origUpdatePickRef = reinterpret_cast<UpdatePickRefFn>(origUpr);
            logger::info("VanillaUISuppressor: installed ActivateChoiceListener::UpdatePickRef "
                         "diagnostic detour (slot {})", PrismaUI::Engine::kActivateChoiceUpdatePickRefSlot);
        }

        g_captureEnabled.store(a_enable);
        logger::info("VanillaUISuppressor::EnableActivateChoiceFilter(enable={}, dropDefaultTake={})",
                     a_enable, a_dropDefaultTake);
    }

    void SuppressActivateChoicePerk(std::uint32_t a_perkFormID, bool a_suppress)
    {
        if (a_suppress) WarnFilteringUnimplemented("SuppressActivateChoicePerk");
        logger::info("VanillaUISuppressor::SuppressActivateChoicePerk({:#010x}, {}) -- no-op, filtering "
                     "is not implemented", a_perkFormID, a_suppress);
    }

    bool GetActivateChoiceLabel(std::uint32_t a_buttonIndex, std::string& a_outLabel)
    {
        std::lock_guard lock{ g_choiceMutex };
        if (a_buttonIndex >= g_choiceSlots.size() || !g_choiceSlots[a_buttonIndex].valid) return false;
        a_outLabel = g_choiceSlots[a_buttonIndex].label;
        return true;
    }

    bool TriggerActivateChoice(std::uint32_t a_buttonIndex)
    {

        static std::once_flag s_disabledWarnOnce;
        std::call_once(s_disabledWarnOnce, [] {
            logger::warn("VanillaUISuppressor::TriggerActivateChoice: dispatch is DISABLED -- the raw "
                         "engine listener cannot be lifetime-guaranteed. Returning false; capture and "
                         "GetActivateChoiceLabel remain active.");
        });
        (void)a_buttonIndex;
        return false;
    }
}
