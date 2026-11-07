#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace PrismaUI::TextureOverlayGeometry {

struct Rect {
    int l = 0;
    int t = 0;
    int r = 0;
    int b = 0;
};

inline bool IsIntValue(double v) {
    return std::isfinite(v) && v >= std::numeric_limits<int>::lowest() && v <= std::numeric_limits<int>::max();
}

inline std::optional<Rect> ParseRect(double x, double y, double w, double h) {
    if (!IsIntValue(x) || !IsIntValue(y) || !IsIntValue(w) || !IsIntValue(h) || w < 0 || h < 0) return std::nullopt;
    const std::int64_t r = static_cast<std::int64_t>(x) + static_cast<std::int64_t>(w);
    const std::int64_t b = static_cast<std::int64_t>(y) + static_cast<std::int64_t>(h);
    if (r > std::numeric_limits<int>::max() || b > std::numeric_limits<int>::max()) return std::nullopt;
    return Rect{static_cast<int>(x), static_cast<int>(y), static_cast<int>(r), static_cast<int>(b)};
}

}
