#pragma once

#include <cstdint>
#include <string>

namespace PrismaUI::Engine::ShellCallFormat {

inline std::string Arg(std::uint64_t v) { return std::to_string(v); }
inline std::string Arg(int v) { return std::to_string(v); }

inline std::string Arg(const std::string& rawJs) { return rawJs; }

template <class... Args>
std::string Call(const char* fn, const Args&... args) {
    std::string js = "window.__prismaShell && window.__prismaShell.";
    js += fn;
    js += '(';
    const char* sep = "";
    (void)sep;
    (((js += sep), (js += Arg(args)), (sep = ",")), ...);
    js += ");";
    return js;
}

}
