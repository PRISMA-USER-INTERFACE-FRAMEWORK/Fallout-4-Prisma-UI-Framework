#include "PrismaUI/TranslationsLanguage.h"

namespace PrismaUI::Translations {

    std::vector<std::string> LanguageIniCandidates(bool vr, const std::string& docsDir, const std::string& gameDir) {
        const std::string gameIni = gameDir + "\\Fallout4.ini";
        if (docsDir.empty()) return {gameIni};
        const std::string profile = docsDir + (vr ? "\\My Games\\Fallout4VR\\" : "\\My Games\\Fallout4\\");
        std::vector<std::string> candidates{vr ? gameIni : profile + "Fallout4.ini"};
        candidates.push_back(profile + "Fallout4Custom.ini");
        // ponytail: Data\<plugin>.ini is not read; add it once the plugin filter and order are RE-reviewed
        return candidates;
    }

    size_t LastNonEmptyIndex(const std::vector<std::string>& values) {
        for (size_t index = values.size(); index-- > 0;) {
            if (!values[index].empty()) return index;
        }
        return values.size();
    }

}
