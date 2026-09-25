#pragma once

#include <filesystem>

namespace PrismaUI::Utils
{

    inline std::filesystem::path GetBasePath()
    {
        return std::filesystem::current_path() / "Data" / "PrismaUI_F4";
    }
}
