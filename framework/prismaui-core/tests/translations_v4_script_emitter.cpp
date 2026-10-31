#include <iostream>
#include <map>
#include <string>

#include "PrismaUI/TranslationsScript.h"

int main() {
    const std::map<std::string, std::string> translations{
        {"greeting", "Hola {{name}}"},
        {"count", "{{n}}/{{empty}}"},
        {"t", "T"},
        {"__proto__", "Proto"},
        {"braces", "literal {x} and {{missing}}"},
        {"unicode", "Café {{name}}"},
    };
    std::cout << PrismaUI::Translations::BuildV4L10NScript(translations, "es");
    return 0;
}
