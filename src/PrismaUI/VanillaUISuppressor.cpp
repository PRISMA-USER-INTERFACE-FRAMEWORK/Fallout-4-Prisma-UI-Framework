#include "VanillaUISuppressor.h"
#include "KnownMenusLog.h"
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
        std::unordered_set<std::string>                 g_explicitlySuppressedWidgets;
        std::atomic<bool>                               g_prismaFocusSuppressionActive{ false };

        std::mutex                       g_menuMutex;
        std::unordered_set<std::string>  g_suppressedMenus;

        std::unordered_map<std::string, PRISMA_UI_API::MenuSuppressPredicate> g_conditionalMenus;

        bool IsFocusSuppressedMenu(std::string_view a_name) noexcept
        {
            return a_name == "BarterMenu" || a_name == "DialogueMenu";
        }

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
                const bool focusSuppress = g_prismaFocusSuppressionActive.load(std::memory_order_acquire) &&
                                           IsFocusSuppressedMenu(a_event.menuName.c_str());
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
                if (focusSuppress) {
                    ForceCloseMenuNow(a_event.menuName);
                    logger::info("VanillaUISuppressor: Prisma focus active, force-closed '{}'", a_event.menuName.c_str());
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

    static bool ApplyHUDWidgetSuppression(const char* a_className, bool a_suppress, bool a_updateExplicit)
    {
        if (!a_className) return false;

        const auto runtime = REX::FModule::GetExecutingModule().GetFileVersion();
        if (!PrismaUI::RuntimeSupportPolicy::IsSupportedGameVersion(
                runtime.major(), runtime.minor(), runtime.patch())) {
            logger::warn("VanillaUISuppressor::SuppressHUDWidget('{}'): refused -- only Fallout 4 "
                         "OG 1.10.163, AE 1.11.137 or later, and NG 1.10.984 are supported",
                         a_className);
            return false;
        }

        if (!REX::FModule::IsRuntimeOG() && !REX::FModule::IsRuntimeAE() && !REX::FModule::IsRuntimeNG()) {
            logger::warn("VanillaUISuppressor::SuppressHUDWidget('{}'): refused -- HUD widget vtables are "
                         "only answered for OG 1.10.163, AE and NG 1.10.984",
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
        const auto commitExplicit = [&] {
            if (!a_updateExplicit) return;
            if (a_suppress) g_explicitlySuppressedWidgets.insert(key);
            else g_explicitlySuppressedWidgets.erase(key);
        };
        const bool explicitAfter = a_updateExplicit ? a_suppress : g_explicitlySuppressedWidgets.contains(key);
        const bool focusSuppress = key == "HUDEnemyHealthMeter" &&
                                   g_prismaFocusSuppressionActive.load(std::memory_order_acquire);
        const bool shouldSuppress = explicitAfter || focusSuppress;
        auto it = g_originalWidgetVfuncs.find(key);

        if (shouldSuppress) {
            if (it != g_originalWidgetVfuncs.end()) {

                const std::uintptr_t currentValue = *reinterpret_cast<std::uintptr_t*>(slotAddr);
                const std::uintptr_t ourDetour = reinterpret_cast<std::uintptr_t>(&AlwaysHidden);
                if (currentValue == ourDetour) {
                    commitExplicit();
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
            commitExplicit();
            g_originalWidgetVfuncs.emplace(std::move(key), original);
            logger::info("VanillaUISuppressor: suppressed HUD widget '{}' (vtable slot {} patched)",
                         a_className, PrismaUI::Engine::kHUDCanBeVisibleSlot);
            return true;
        } else {
            if (it == g_originalWidgetVfuncs.end()) {
                commitExplicit();
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
            commitExplicit();
            g_originalWidgetVfuncs.erase(it);
            logger::info("VanillaUISuppressor: restored HUD widget '{}' (vtable slot {} reverted)",
                         a_className, PrismaUI::Engine::kHUDCanBeVisibleSlot);
            return true;
        }
    }

    bool SuppressHUDWidget(const char* a_className, bool a_suppress)
    {
        return ApplyHUDWidgetSuppression(a_className, a_suppress, true);
    }

    void SetPrismaUIFocusActive(bool a_active)
    {
        const bool previous = g_prismaFocusSuppressionActive.exchange(a_active, std::memory_order_acq_rel);
        if (previous == a_active) return;

        if (a_active) {
            (void)CloseVanillaMenu("BarterMenu");
            (void)CloseVanillaMenu("DialogueMenu");
        }

        const bool hudResult = ApplyHUDWidgetSuppression("HUDEnemyHealthMeter", false, false);
        logger::info("VanillaUISuppressor: Prisma focus suppression {} (enemy health meter {})",
                     a_active ? "enabled" : "disabled", hudResult ? "updated" : "update failed");
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
        if (a_suppress) {
            KnownMenus::LogMenuPolicyOnce("SuppressVanillaMenu", a_menuName, KnownMenus::Mechanism::Hide);
        }
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
        if (a_predicate) {
            KnownMenus::LogMenuPolicyOnce("SuppressVanillaMenuIf", a_menuName, KnownMenus::Mechanism::ConditionalClose);
        }
        std::lock_guard lock{ g_menuMutex };
        if (a_predicate) {
            g_conditionalMenus[a_menuName] = a_predicate;
            logger::info("VanillaUISuppressor: SuppressVanillaMenuIf('{}') registered", a_menuName);
        } else {
            g_conditionalMenus.erase(a_menuName);
            logger::info("VanillaUISuppressor: SuppressVanillaMenuIf('{}') unregistered", a_menuName);
        }
    }
}
