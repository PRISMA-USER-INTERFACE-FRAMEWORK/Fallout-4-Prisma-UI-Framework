#pragma once

#include <map>
#include <string>

namespace PrismaUI::Translations {

    bool ParseJsonLocale(const std::string& json, std::map<std::string, std::string>& result);
    void MergeJsonLocale(const std::map<std::string, std::string>& locale,
                         std::map<std::string, std::string>& result);

}
