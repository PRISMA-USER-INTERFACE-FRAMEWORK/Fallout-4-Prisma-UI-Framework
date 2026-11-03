#include "FocusMenu.h"

#include <atomic>
#include <cstdint>

namespace {

    std::atomic<bool> g_registered{ false };
    std::atomic<bool> g_openRequested{ false };
    std::atomic<bool> g_reconcileRequested{ false };
    std::atomic<bool> g_reconcileTaskPending{ false };
    std::atomic<std::uint64_t> g_requestGeneration{ 0 };
}

RE::IMenu* FocusMenu::Creator([[maybe_unused]] const RE::UIMessage& a_message)
{
    if (!g_openRequested.load(std::memory_order_acquire)) {
        logger::debug("FocusMenu::Creator: stale kShow rejected");
        return nullptr;
    }

    auto menu = new FocusMenu();
    if (!menu->IsValid()) {
        delete menu;
        return nullptr;
    }
    return menu;
}

FocusMenu::FocusMenu()
{
    using Context  = RE::UserEvents::INPUT_CONTEXT_ID;
    using MenuFlag = RE::UI_MENU_FLAGS;

    auto scaleformManager = RE::BSScaleformManager::GetSingleton();
    if (!scaleformManager) {
        logger::error("FocusMenu: BSScaleformManager singleton is null");
        return;
    }

    const bool success = scaleformManager->LoadMovieEx(*this, "Interface/CursorMenu.swf");

    if (!success || !this->uiMovie) {
        logger::error("FocusMenu: Failed to load Interface/CursorMenu.swf");
        return;
    }

    uiMovie->SetVisible(false);

    this->menuFlags.set(
        MenuFlag::kUsesCursor,
        MenuFlag::kModal,
        MenuFlag::kAllowSaving,
        MenuFlag::kAdvancesUnderPauseMenu,
        MenuFlag::kRendersUnderPauseMenu);

    this->depthPriority = static_cast<RE::UI_DEPTH_PRIORITY>(13);
    this->inputContext  = Context::kBasicMenuNav;
}

void FocusMenu::AdvanceMovie([[maybe_unused]] float a_interval,
                              [[maybe_unused]] std::uint64_t a_currentTime)
{}

RE::UI_MESSAGE_RESULTS FocusMenu::ProcessMessage(RE::UIMessage& a_message)
{
    return RE::IMenu::ProcessMessage(a_message);
}

void FocusMenu::OnRemovedFromMenuStack()
{
    RE::IMenu::OnRemovedFromMenuStack();

    if (!g_openRequested.load(std::memory_order_acquire)) {
        return;
    }

    g_reconcileRequested.store(true, std::memory_order_release);

    auto* ui = RE::UI::GetSingleton();
    if (!ui || ui->closingAllMenus) {
        logger::debug("FocusMenu::OnRemovedFromMenuStack: reopen deferred while UI is closing menus");
        return;
    }

    auto* msgQ = RE::UIMessageQueue::GetSingleton();
    if (!msgQ) {
        logger::warn("FocusMenu::OnRemovedFromMenuStack: UIMessageQueue is null");
        return;
    }

    logger::debug("FocusMenu::OnRemovedFromMenuStack: stale hide detected, sending kShow");
    msgQ->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kShow);
    g_reconcileRequested.store(false, std::memory_order_release);
}

bool FocusMenu::IsOpen()
{
    auto ui = RE::UI::GetSingleton();
    return ui && ui->GetMenuOpen(MENU_NAME);
}

void FocusMenu::Open(bool pauseGame)
{
    logger::debug("FocusMenu::Open requested (pauseGame={}, pause owned by PauseHold)", pauseGame);
    const auto* tasks = F4SE::GetTaskInterface();
    if (!tasks) {
        logger::error("FocusMenu::Open: F4SE task interface is unavailable");
        return;
    }

    g_openRequested.store(true, std::memory_order_release);
    g_reconcileRequested.store(false, std::memory_order_release);
    const auto requestGeneration = g_requestGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;

    tasks->AddUITask([=] {
        if (g_requestGeneration.load(std::memory_order_acquire) != requestGeneration) {
            logger::debug("FocusMenu::Open: superseded request, skipping");
            return;
        }

        auto* ui = RE::UI::GetSingleton();
        if (!ui) {
            g_reconcileRequested.store(true, std::memory_order_release);
            logger::warn("FocusMenu::Open: UI singleton is null");
            return;
        }

        bool expected = false;
        if (g_registered.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            ui->RegisterMenu(MENU_NAME.data(), Creator);
            logger::info("FocusMenu::Open: registered {}", MENU_NAME);
        }

        if (ui->closingAllMenus) {
            g_reconcileRequested.store(true, std::memory_order_release);
            logger::debug("FocusMenu::Open: show deferred while UI is closing menus");
            return;
        }

        auto* msgQ = RE::UIMessageQueue::GetSingleton();
        if (!msgQ) {
            g_reconcileRequested.store(true, std::memory_order_release);
            logger::warn("FocusMenu::Open: UIMessageQueue is null");
            return;
        }
        if (IsOpen()) {
            logger::debug("FocusMenu::Open: already open, skipping");
            return;
        }

        logger::debug("FocusMenu::Open: sending kShow");
        msgQ->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kShow);
    });
}

void FocusMenu::Close()
{
    logger::debug("FocusMenu::Close requested");
    g_openRequested.store(false, std::memory_order_release);
    g_reconcileRequested.store(false, std::memory_order_release);
    const auto requestGeneration = g_requestGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;

    const auto* tasks = F4SE::GetTaskInterface();
    if (!tasks) {
        logger::error("FocusMenu::Close: F4SE task interface is unavailable");
        return;
    }

    tasks->AddUITask([=] {
        if (g_requestGeneration.load(std::memory_order_acquire) != requestGeneration) {
            logger::debug("FocusMenu::Close: superseded request, skipping");
            return;
        }

        auto* msgQ = RE::UIMessageQueue::GetSingleton();
        if (!msgQ) {
            logger::warn("FocusMenu::Close: UIMessageQueue is null");
            return;
        }

        if (IsOpen()) {
            logger::debug("FocusMenu::Close: sending kHide");
            msgQ->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kHide);
        } else {
            logger::debug("FocusMenu::Close: already closed; stale kShow will be rejected by Creator");
        }
    });
}

void FocusMenu::Tick()
{
    if (!g_reconcileRequested.load(std::memory_order_acquire)) {
        return;
    }

    bool expected = false;
    if (!g_reconcileTaskPending.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    const auto* tasks = F4SE::GetTaskInterface();
    if (!tasks) {
        g_reconcileTaskPending.store(false, std::memory_order_release);
        logger::warn("FocusMenu::Tick: F4SE task interface is unavailable");
        return;
    }

    tasks->AddUITask([] {
        g_reconcileTaskPending.store(false, std::memory_order_release);

        if (!g_reconcileRequested.load(std::memory_order_acquire)) {
            return;
        }
        if (!g_openRequested.load(std::memory_order_acquire)) {
            g_reconcileRequested.store(false, std::memory_order_release);
            return;
        }

        auto* ui = RE::UI::GetSingleton();
        if (!ui || ui->closingAllMenus) {
            return;
        }

        bool expectedRegistration = false;
        if (g_registered.compare_exchange_strong(expectedRegistration, true, std::memory_order_acq_rel)) {
            ui->RegisterMenu(MENU_NAME.data(), Creator);
            logger::info("FocusMenu::Tick: registered {}", MENU_NAME);
        }

        if (IsOpen()) {
            g_reconcileRequested.store(false, std::memory_order_release);
            return;
        }

        auto* msgQ = RE::UIMessageQueue::GetSingleton();
        if (!msgQ) {
            return;
        }

        logger::debug("FocusMenu::Tick: deferred reopen sending kShow");
        msgQ->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kShow);
        g_reconcileRequested.store(false, std::memory_order_release);
    });
}
