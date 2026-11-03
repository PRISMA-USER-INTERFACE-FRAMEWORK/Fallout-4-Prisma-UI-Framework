#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace PrismaUI::ClearColorPolicy {

inline std::string AlphaText(std::uint32_t alpha) {
    const std::uint32_t thousandths = (alpha * 1000 + 127) / 255;
    if (thousandths == 0) return "0";
    if (thousandths >= 1000) return "1";
    char digits[4];
    std::snprintf(digits, sizeof digits, "%03u", thousandths);
    std::string fraction = digits;
    while (fraction.back() == '0') fraction.pop_back();
    return "0." + fraction;
}

inline std::string BackgroundScript(std::uint32_t argb) {
    if (argb == 0) {
        return "(function(){var s=document.getElementById('__prismaBackground');"
               "if(s&&s.parentNode)s.parentNode.removeChild(s);})();";
    }
    const std::string css = ":root{background-color:rgba(" + std::to_string((argb >> 16) & 0xFF) + "," +
                            std::to_string((argb >> 8) & 0xFF) + "," + std::to_string(argb & 0xFF) + "," +
                            AlphaText(argb >> 24) + ")}";
    return "(function(){var apply=function(){var d=document,h=d.head||d.documentElement,"
           "s=d.getElementById('__prismaBackground');"
           "if(!s){s=d.createElement('style');s.id='__prismaBackground';h.insertBefore(s,h.firstChild);}"
           "s.textContent='" + css + "';};"
           "if(document.documentElement)apply();else document.addEventListener('DOMContentLoaded',apply);})();";
}

}
