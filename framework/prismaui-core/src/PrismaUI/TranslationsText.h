#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace PrismaUI::Translations {

    void ParseBethesdaTranslation(std::string_view raw, std::unordered_map<std::string, std::string>& result);

}
