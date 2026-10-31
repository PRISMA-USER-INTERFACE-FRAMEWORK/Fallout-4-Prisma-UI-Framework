#include "PrismaUI/ClearColorPolicy.h"

#include <cstdint>
#include <iostream>
#include <string>

namespace {
int failures = 0;
using namespace PrismaUI::ClearColorPolicy;

void Expect(bool condition, std::uint32_t argb, const char* what) {
    if (condition) return;
    std::cerr << std::hex << "FAIL: argb 0x" << argb << std::dec << ": " << what << "\n";
    ++failures;
}

void ExpectContains(const std::string& script, const char* text, std::uint32_t argb) {
    Expect(script.find(text) != std::string::npos, argb, text);
}

void ExpectAbsent(const std::string& script, const char* text, std::uint32_t argb) {
    Expect(script.find(text) == std::string::npos, argb, text);
}
}  // namespace

int main() {
    const std::uint32_t colors[] = {0x80804020, 0xFF000000, 0xFFFFFFFF, 0x00000000, 0x00FFFFFF};
    for (const auto argb : colors) {
        ExpectContains(BackgroundScript(argb), "__prismaBackground", argb);
    }

    const std::string masked = BackgroundScript(0x80804020);
    ExpectContains(masked, "rgba(128,64,32,0.502)", 0x80804020);
    ExpectContains(masked, ":root{background-color:", 0x80804020);
    ExpectContains(BackgroundScript(0xFF000000), "rgba(0,0,0,1)", 0xFF000000);
    ExpectContains(BackgroundScript(0xFFFFFFFF), "rgba(255,255,255,1)", 0xFFFFFFFF);
    ExpectContains(BackgroundScript(0x00FFFFFF), "rgba(255,255,255,0)", 0x00FFFFFF);

    const std::string removal = BackgroundScript(0);
    ExpectAbsent(removal, "background-color", 0);
    ExpectContains(removal, "removeChild", 0);

    if (failures != 0) {
        std::cerr << failures << " clear-color policy test(s) failed\n";
        return 1;
    }
    std::cout << "clear-color policy tests passed\n";
    return 0;
}
