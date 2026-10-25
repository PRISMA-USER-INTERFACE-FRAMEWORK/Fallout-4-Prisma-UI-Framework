#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace PrismaUI::Translations {

    std::vector<std::string> LanguageIniCandidates(bool vr, const std::string& docsDir, const std::string& gameDir);
    size_t LastNonEmptyIndex(const std::vector<std::string>& values);

}
