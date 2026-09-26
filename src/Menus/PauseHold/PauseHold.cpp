#include "PauseHold.h"
#include "PauseHoldPolicy.h"

namespace {

    PrismaUI::PauseHoldPolicy::State g_policy;
    std::atomic<bool> g_reconcileTaskPending{false};
}

RE::IMenu* PauseHold::Creator([[maybe_unused]] const RE::UIMessage& a_message)
{
    if (!g_policy.TargetPaused()) {
        logger::debug("PauseHold::Creator: stale kShow rejected");
        return nullptr;
    }

    auto menu = new PauseHold();
    if (!menu->IsValid()) {
        delete menu;
        return nullptr;
    }
    return menu;
}

PauseHold::PauseHold()
{
    using MenuFlag = RE::UI_MENU_FLAGS;

    auto scaleformManager = RE::BSScaleformManager::GetSingleton();
    if (!scaleformManager) {
        logger::error("PauseHold: BSScaleformManager singleton is null");
        return;
    }

    if (!scaleformManager->LoadMovieEx(*this, "Interface/CursorMenu.swf") || !this->uiMovie) {
        logger::error("PauseHold: failed to load Interface/CursorMenu.swf");
        return;
    }

    uiMovie->SetVisible(false);

    this->menuFlags.set(
        MenuFlag::kPausesGame,
        MenuFlag::kAllowSaving,
        MenuFlag::kAdvancesUnderPauseMenu,
        MenuFlag::kRendersUnderPauseMenu);
}

void PauseHold::AdvanceMovie([[maybe_unused]] float a_interval,
                             [[maybe_unused]] std::uint64_t a_currentTime)
{}

RE::UI_MESSAGE_RESULTS PauseHold::ProcessMessage(RE::UIMessage& a_message)
{
    return RE::IMenu::ProcessMessage(a_message);
}

void PauseHold::OnRemovedFromMenuStack()
{
    RE::IMenu::OnRemovedFromMenuStack();

    if (!g_policy.TargetPaused()) {
        return;
    }

    g_policy.InvalidateMessageState();

    auto* ui = RE::UI::GetSingleton();
    if (!ui || ui->closingAllMenus) {
        logger::debug("PauseHold::OnRemovedFromMenuStack: reopen deferred while UI is closing menus");
        return;
    }

    auto* msgQ = RE::UIMessageQueue::GetSingleton();
    if (!msgQ) {
        logger::warn("PauseHold::OnRemovedFromMenuStack: UIMessageQueue is null");
        return;
    }

    logger::debug("PauseHold::OnRemovedFromMenuStack: stale hide detected, sending kShow");
    msgQ->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kShow);
    g_policy.MarkMessageSent(true);
}

bool PauseHold::IsOpen()
{
    auto ui = RE::UI::GetSingleton();
    return ui && ui->GetMenuOpen(MENU_NAME);
}

bool PauseHold::IsRequested()
{

    return g_policy.TargetPaused();
}

void PauseHold::Set(bool paused)
{
    g_policy.SetTarget(paused);

    auto* tasks = F4SE::GetTaskInterface();
    if (!tasks) {
        logger::error("PauseHold::Set: F4SE task interface is unavailable");
        return;
    }

    tasks->AddUITask([] {
        bool want = false;
        if (!g_policy.ShouldSendMessage(want)) {
            return;
        }

        auto* ui = RE::UI::GetSingleton();
        if (!ui || ui->closingAllMenus) {
            g_policy.InvalidateMessageState();
            return;
        }

        auto msgQ = RE::UIMessageQueue::GetSingleton();
        if (!msgQ) {
            g_policy.InvalidateMessageState();
            return;
        }

        if (PauseHold::IsOpen() != want) {
            msgQ->AddMessage(PauseHold::MENU_NAME,
                             want ? RE::UI_MESSAGE_TYPE::kShow : RE::UI_MESSAGE_TYPE::kHide);
        }

        g_policy.MarkMessageSent(want);
    });
}

void PauseHold::Tick()
{
    bool pendingValue = false;
    if (g_policy.ShouldSendMessage(pendingValue)) {
        bool expected = false;
        if (!g_reconcileTaskPending.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            return;
        }

        auto* tasks = F4SE::GetTaskInterface();
        if (!tasks) {
            g_reconcileTaskPending.store(false, std::memory_order_release);
            logger::warn("PauseHold::Tick: F4SE task interface is unavailable");
            return;
        }

        tasks->AddUITask([] {
            g_reconcileTaskPending.store(false, std::memory_order_release);

            bool want = false;
            if (!g_policy.ShouldSendMessage(want)) {
                return;
            }

            auto* ui = RE::UI::GetSingleton();
            if (!ui || ui->closingAllMenus) {
                return;
            }

            auto* msgQ = RE::UIMessageQueue::GetSingleton();
            if (!msgQ) {
                return;
            }

            if (PauseHold::IsOpen() != want) {
                msgQ->AddMessage(PauseHold::MENU_NAME,
                                 want ? RE::UI_MESSAGE_TYPE::kShow : RE::UI_MESSAGE_TYPE::kHide);
            }
            g_policy.MarkMessageSent(want);
        });
        return;
    }

    if (!g_policy.AdvanceVerificationFrame()) {
        return;
    }

    auto* tasks = F4SE::GetTaskInterface();
    if (!tasks) {
        logger::warn("PauseHold::Tick: F4SE task interface is unavailable");
        return;
    }

    tasks->AddUITask([] {
        const bool want = g_policy.TargetPaused();
        const bool open = PauseHold::IsOpen();
        if (open == want) {

            logger::info("PauseHold: settled at paused={}", want);
        } else {
            logger::warn("PauseHold: asked for paused={} but the menu is {}", want,
                         open ? "open" : "closed");
            g_policy.InvalidateMessageState();
        }
    });
}
