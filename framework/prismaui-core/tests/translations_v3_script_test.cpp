#include <cassert>
#include <string>
#include <unordered_map>

#include "PrismaUI/TranslationsScript.h"

int main() {
    const std::unordered_map<std::string, std::string> translations{
        {"controls", "tab\tback\bform\f"},
    };
    const std::string expected =
        "window.L10N={\"controls\":\"tab\tback\bform\f\"};"
        "window.t=function(k){return window.L10N[k]!==undefined?window.L10N[k]:k;};";
    assert(PrismaUI::Translations::BuildL10NScript(translations) == expected);
    return 0;
}
