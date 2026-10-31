#include <cassert>
#include <string>
#include <unordered_map>

#include "PrismaUI/TranslationsText.h"

int main() {
    std::unordered_map<std::string, std::string> translations;
    PrismaUI::Translations::ParseBethesdaTranslation(
        "$HELLO\tReady\r\n$COUNT\t2\nnot-a-translation\n$NO_TAB", translations);
    assert(translations.size() == 2);
    assert(translations.at("$HELLO") == "Ready");
    assert(translations.at("$COUNT") == "2");

    std::string utf16;
    utf16.push_back(static_cast<char>(0xFF));
    utf16.push_back(static_cast<char>(0xFE));
    const std::u16string source = u"$HELLO\tReady\r\n$COUNT\t2";
    for (const char16_t value : source) {
        utf16.push_back(static_cast<char>(value & 0xFF));
        utf16.push_back(static_cast<char>(value >> 8));
    }
    PrismaUI::Translations::ParseBethesdaTranslation(utf16, translations);
    assert(translations.size() == 2);
    assert(translations.at("$HELLO") == "Ready");
    assert(translations.at("$COUNT") == "2");

    PrismaUI::Translations::ParseBethesdaTranslation("x", translations);
    assert(translations.empty());
    return 0;
}
