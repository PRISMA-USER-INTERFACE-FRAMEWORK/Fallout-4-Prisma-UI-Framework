#include <cassert>
#include <string>
#include <vector>

#include "PrismaUI/TranslationsLanguage.h"

int main() {
    using PrismaUI::Translations::LanguageIniCandidates;
    using PrismaUI::Translations::LastNonEmptyIndex;

    const std::string docs = "C:\\Users\\Ricky\\Documents";
    const std::string game = "D:\\Games\\Fallout4";

    assert((LanguageIniCandidates(false, docs, game) ==
            std::vector<std::string>{docs + "\\My Games\\Fallout4\\Fallout4.ini",
                                     docs + "\\My Games\\Fallout4\\Fallout4Custom.ini"}));
    assert((LanguageIniCandidates(false, "", game) == std::vector<std::string>{game + "\\Fallout4.ini"}));
    assert((LanguageIniCandidates(true, docs, game) ==
            std::vector<std::string>{game + "\\Fallout4.ini", docs + "\\My Games\\Fallout4VR\\Fallout4Custom.ini"}));
    assert((LanguageIniCandidates(true, "", game) == std::vector<std::string>{game + "\\Fallout4.ini"}));

    assert(LastNonEmptyIndex({"english", "french"}) == 1);
    assert(LastNonEmptyIndex({"french", ""}) == 0);
    assert(LastNonEmptyIndex({"", ""}) == 2);
    assert(LastNonEmptyIndex({}) == 0);
    return 0;
}
