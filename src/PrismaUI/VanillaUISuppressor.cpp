#include "VanillaUISuppressor.h"
#include "../Engine/EngineVanillaUI.h"
#include "../RuntimeSupportPolicy.h"

#include <RE/U/UI.h>
#include <RE/U/UIMessageQueue.h>
#include <RE/M/MenuOpenCloseEvent.h>
#include <RE/C/Console.h>
#include <RE/T/TESObjectREFR.h>
#include <RE/T/TESBoundObject.h>
#include <RE/T/TESForm.h>
#include <REL/Offset.h>
#include <REL/Relocation.h>
#include <REX/FModule.h>

#include <Windows.h>

#include <atomic>
#include <mutex>
#include <set>
#include <utility>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace PrismaUI::VanillaUISuppressor
{
    namespace
    {

        bool AlwaysHidden(void*) { return false; }

        std::mutex                                        g_widgetMutex;
        std::unordered_map<std::string, std::uintptr_t>   g_originalWidgetVfuncs;

        std::mutex                       g_menuMutex;
        std::unordered_set<std::string>  g_suppressedMenus;

        std::unordered_map<std::string, PRISMA_UI_API::MenuSuppressPredicate> g_conditionalMenus;

        void ApplyMenuVisibility(const RE::BSFixedString& a_name, bool a_visible)
        {
            auto* ui = RE::UI::GetSingleton();
            if (!ui) return;
            auto menu = ui->GetMenu(a_name);
            if (!menu || !menu->uiMovie) return;
            menu->uiMovie->SetVisible(a_visible);
        }

        void ForceCloseMenuNow(const RE::BSFixedString& a_name)
        {

            ApplyMenuVisibility(a_name, false);
            if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
                queue->AddMessage(a_name, RE::UI_MESSAGE_TYPE::kHide);
            }
        }

        class VanillaMenuSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
        {
        public:
            RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& a_event,
                                                   RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
            {
                if (!a_event.opening) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                bool suppress = false;
                bool firstSeen = false;
                PRISMA_UI_API::MenuSuppressPredicate predicate = nullptr;
                {
                    std::lock_guard lock{ g_menuMutex };

                    static std::set<std::string> s_seenMenus;
                    const std::string menuName{ a_event.menuName.c_str() };
                    firstSeen = s_seenMenus.insert(menuName).second;
                    suppress = g_suppressedMenus.contains(menuName);
                    if (auto it = g_conditionalMenus.find(menuName); it != g_conditionalMenus.end()) {
                        predicate = it->second;
                    }
                }

                if (firstSeen) {
                    logger::info("VanillaUISuppressor: menu opened (first time this session): '{}'",
                                 a_event.menuName.c_str());
                }
                if (suppress) {
                    ApplyMenuVisibility(a_event.menuName, false);
                    logger::info("VanillaUISuppressor: reapplied suppression to '{}' on reopen",
                                 a_event.menuName.c_str());
                }
                if (predicate && predicate()) {
                    ForceCloseMenuNow(a_event.menuName);
                    logger::info("VanillaUISuppressor: predicate true, force-closed '{}' on open",
                                 a_event.menuName.c_str());
                }
                return RE::BSEventNotifyControl::kContinue;
            }

            static VanillaMenuSink* GetSingleton()
            {
                static VanillaMenuSink singleton;
                return std::addressof(singleton);
            }
        };

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

    void Install()
    {
        if (auto* ui = RE::UI::GetSingleton()) {
            ui->RegisterSink(VanillaMenuSink::GetSingleton());
            logger::info("VanillaUISuppressor: registered menu-open sink for auto-reapply");
        } else {
            logger::warn("VanillaUISuppressor: RE::UI singleton unavailable, menu auto-reapply not installed");
        }
    }

    bool SuppressHUDWidget(const char* a_className, bool a_suppress)
    {
        if (!a_className) return false;

        const auto runtime = REX::FModule::GetExecutingModule().GetFileVersion();
        if (!PrismaUI::RuntimeSupportPolicy::IsSupportedGameVersion(
                runtime.major(), runtime.minor(), runtime.patch())) {
            logger::warn("VanillaUISuppressor::SuppressHUDWidget('{}'): refused -- only Fallout 4 "
                         "OG 1.10.163 and AE 1.11.137 or later are supported",
                         a_className);
            return false;
        }

        const std::uintptr_t vtableAddress = PrismaUI::Engine::ResolveHUDWidgetVtable(a_className);
        if (vtableAddress == 0) {
            logger::warn("VanillaUISuppressor::SuppressHUDWidget('{}'): no named CommonLib vtable "
                         "authority is available", a_className);
            return false;
        }

        REL::Relocation<std::uintptr_t> vtable{ vtableAddress };

        const auto rdataSection = REX::FModule::GetExecutingModule().GetSection(".rdata");
        const std::uintptr_t vtableAddr = vtable.address();
        if (rdataSection.GetSize() == 0 || vtableAddr < rdataSection.GetAddress() ||
            vtableAddr >= rdataSection.GetAddress() + rdataSection.GetSize()) {
            logger::critical("VanillaUISuppressor::SuppressHUDWidget('{}'): REFUSING to patch -- "
                             "resolved vtable address {:#x} (RVA {:#x}) does not fall inside .rdata "
                             "({:#x}..{:#x}). This Address Library ID is almost certainly wrong; not "
                             "applying.",
                             a_className, vtableAddr, vtable.offset(), rdataSection.GetAddress(),
                             rdataSection.GetAddress() + rdataSection.GetSize());
            return false;
        }

        const std::uintptr_t slotAddr =
            vtable.address() + (sizeof(void*) * PrismaUI::Engine::kHUDCanBeVisibleSlot);

        std::lock_guard lock{ g_widgetMutex };
        std::string key{ a_className };
        auto it = g_originalWidgetVfuncs.find(key);

        if (a_suppress) {
            if (it != g_originalWidgetVfuncs.end()) {

                const std::uintptr_t currentValue = *reinterpret_cast<std::uintptr_t*>(slotAddr);
                const std::uintptr_t ourDetour = reinterpret_cast<std::uintptr_t>(&AlwaysHidden);
                if (currentValue == ourDetour) {
                    return true;
                }
                logger::warn("VanillaUISuppressor::SuppressHUDWidget('{}'): a record exists but slot {} "
                             "now holds {:#x}, not our detour ({:#x}); another hook owns it. Not "
                             "re-patching and not claiming suppression is active.",
                             a_className, PrismaUI::Engine::kHUDCanBeVisibleSlot, currentValue, ourDetour);
                return false;
            }

            const auto textSection = REX::FModule::GetExecutingModule().GetSection(".text");
            const std::uintptr_t currentValue = *reinterpret_cast<std::uintptr_t*>(slotAddr);
            if (currentValue < textSection.GetAddress() ||
                currentValue >= textSection.GetAddress() + textSection.GetSize()) {
                logger::critical("VanillaUISuppressor::SuppressHUDWidget('{}'): REFUSING to patch -- "
                                 "current vtable slot 7 value {:#x} at resolved RVA {:#x} does not "
                                 "point inside .text ({:#x}..{:#x}). The named vtable authority is almost "
                                 "certainly wrong; not applying.",
                                 a_className, currentValue, vtable.offset(), textSection.GetAddress(),
                                 textSection.GetAddress() + textSection.GetSize());
                return false;
            }

            const std::uintptr_t original =
                vtable.write_vfunc(PrismaUI::Engine::kHUDCanBeVisibleSlot, &AlwaysHidden);
            g_originalWidgetVfuncs.emplace(std::move(key), original);
            logger::info("VanillaUISuppressor: suppressed HUD widget '{}' (vtable slot {} patched)",
                         a_className, PrismaUI::Engine::kHUDCanBeVisibleSlot);
            return true;
        } else {
            if (it == g_originalWidgetVfuncs.end()) {
                return true;
            }

            const std::uintptr_t currentValue = *reinterpret_cast<std::uintptr_t*>(slotAddr);
            const std::uintptr_t ourDetour = reinterpret_cast<std::uintptr_t>(&AlwaysHidden);
            if (currentValue != ourDetour) {
                logger::warn("VanillaUISuppressor::SuppressHUDWidget('{}'): slot {} no longer holds our "
                             "detour (now {:#x}, expected {:#x}); another hook is present. Keeping our "
                             "ownership record and not restoring, to avoid removing it.",
                             a_className, PrismaUI::Engine::kHUDCanBeVisibleSlot, currentValue, ourDetour);
                return false;
            }
            vtable.write_vfunc(PrismaUI::Engine::kHUDCanBeVisibleSlot, it->second);
            g_originalWidgetVfuncs.erase(it);
            logger::info("VanillaUISuppressor: restored HUD widget '{}' (vtable slot {} reverted)",
                         a_className, PrismaUI::Engine::kHUDCanBeVisibleSlot);
            return true;
        }
    }

    bool SuppressVanillaMenu(const char* a_menuName, bool a_suppress)
    {
        if (!a_menuName) return false;
        RE::BSFixedString name{ a_menuName };

        {
            std::lock_guard lock{ g_menuMutex };
            if (a_suppress) {
                g_suppressedMenus.emplace(a_menuName);
            } else {
                g_suppressedMenus.erase(a_menuName);
            }
        }

        ApplyMenuVisibility(name, !a_suppress);
        logger::info("VanillaUISuppressor: SuppressVanillaMenu('{}', {})", a_menuName, a_suppress);
        return true;
    }

    bool IsMenuSuppressed(const char* a_menuName)
    {
        if (!a_menuName) return false;
        std::lock_guard lock{ g_menuMutex };
        return g_suppressedMenus.contains(a_menuName);
    }

    bool CloseVanillaMenu(const char* a_menuName)
    {
        if (!a_menuName) return false;
        auto* queue = RE::UIMessageQueue::GetSingleton();
        if (!queue) return false;
        queue->AddMessage(RE::BSFixedString{ a_menuName }, RE::UI_MESSAGE_TYPE::kHide);
        logger::info("VanillaUISuppressor: CloseVanillaMenu('{}')", a_menuName);
        return true;
    }

    void SuppressVanillaMenuIf(const char* a_menuName, PRISMA_UI_API::MenuSuppressPredicate a_predicate)
    {
        if (!a_menuName) return;
        std::lock_guard lock{ g_menuMutex };
        if (a_predicate) {
            g_conditionalMenus[a_menuName] = a_predicate;
            logger::info("VanillaUISuppressor: SuppressVanillaMenuIf('{}') registered", a_menuName);
        } else {
            g_conditionalMenus.erase(a_menuName);
            logger::info("VanillaUISuppressor: SuppressVanillaMenuIf('{}') unregistered", a_menuName);
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
                             "4 OG 1.10.163 and AE 1.11.137 or later are supported");
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
                logger::info("VanillaUISuppressor: [RVA CHECK] ActivateChoiceListener (resolved {:#x}) slot1 -> RVA {:#x} "
                             "(expect 0x832870), slot2 -> RVA {:#x} (expect 0x8329a0)",
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
