#pragma once

#pragma warning(push)
#include <RE/Fallout.h>
#include <F4SE/F4SE.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <utility>
#pragma warning(pop)

#include "PrismaUI_F4_API.h"
#include "PrismaUI_F4_Modern_API.h"

namespace logger {
    template<typename... Args>
    inline void info(std::format_string<Args...> fmt, Args&&... args) {
        REX::Impl::Log(std::source_location::current(), REX::ELogLevel::Info, fmt, std::forward<Args>(args)...);
    }
    template<typename... Args>
    inline void warn(std::format_string<Args...> fmt, Args&&... args) {
        REX::Impl::Log(std::source_location::current(), REX::ELogLevel::Warning, fmt, std::forward<Args>(args)...);
    }
}

#define DLLEXPORT __declspec(dllexport)
