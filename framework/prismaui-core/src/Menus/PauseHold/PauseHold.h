#pragma once

#include <RE/Fallout.h>

class PauseHold : public RE::IMenu
{
public:
    static constexpr std::string_view MENU_NAME = "PrismaUI_PauseHold";

    void AdvanceMovie(float a_interval, std::uint64_t a_currentTime) override;
    RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage& a_message) override;
    void OnRemovedFromMenuStack() override;

    static RE::IMenu* Creator(const RE::UIMessage& a_message);

    static void Set(bool paused);
    static bool IsOpen();

    static bool IsRequested();

    static void Tick();

    bool IsValid() const { return uiMovie != nullptr; }

private:
    PauseHold();
};
