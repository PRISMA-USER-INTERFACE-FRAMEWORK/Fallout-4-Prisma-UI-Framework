#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace PrismaUI::ControllerGlyphs {

    enum class Device { kKeyboard, kGamepad };
    enum class Style  { kXbox, kPlayStation };

    void InstallInputTracking();
    bool IsControllerButton(std::uint32_t buttonCode) noexcept;
    bool SetNativeGamepad(std::uint64_t view, bool enabled);

    void   NoteInputDevice(RE::INPUT_DEVICE device);
    bool   UsingGamepad();

    void  SetStyle(Style s);
    Style GetStyle();

    std::string CanonicalFromCode(std::uint32_t bsButtonCode);
    bool CodeFromCanonical(std::string_view name, std::uint32_t& code) noexcept;

    std::string ButtonPrompt(const char* userEvent);
}
