#include "PrismaUI/TextureOverlayGeometry.h"

#include <iostream>
#include <limits>

namespace {
int failures = 0;
void Check(bool value, const char* message) {
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}
using namespace PrismaUI::TextureOverlayGeometry;
constexpr int kMax = std::numeric_limits<int>::max();
constexpr int kMin = std::numeric_limits<int>::lowest();
}  // namespace

int main() {
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();

    Check(!ParseRect(2147483648.0, 0, 0, 0), "x just above int max is rejected");
    Check(!ParseRect(2147483647.0, 0, 1, 0), "x=INT_MAX with w=1 overflows r and is rejected");
    Check(!ParseRect(0, 2147483647.0, 0, 1), "y=INT_MAX with h=1 overflows b and is rejected");
    Check(!ParseRect(nan, 0, 1, 1), "NaN x is rejected");
    Check(!ParseRect(0, inf, 1, 1), "Infinity y is rejected");
    Check(!ParseRect(0, 0, inf, 1), "Infinity w is rejected");
    Check(!ParseRect(1e308, 0, 1, 1), "1e308 x is rejected");
    Check(!ParseRect(0, 0, -1, 1), "negative w is rejected");
    Check(!ParseRect(0, 0, 1, -1), "negative h is rejected");

    const auto typical = ParseRect(10, 20, 30, 40);
    Check(typical && typical->l == 10 && typical->t == 20 && typical->r == 40 && typical->b == 60,
          "typical rect yields l,t,r=x+w,b=y+h");
    const auto edge = ParseRect(2147483646.0, 0, 1, 0);
    Check(edge && edge->l == kMax - 1 && edge->r == kMax, "r exactly INT_MAX is accepted");
    const auto full = ParseRect(0, 0, kMax, kMax);
    Check(full && full->r == kMax && full->b == kMax, "w=h=INT_MAX from origin is accepted");
    const auto lowest = ParseRect(kMin, kMin, 0, 0);
    Check(lowest && lowest->l == kMin && lowest->r == kMin && lowest->t == kMin && lowest->b == kMin,
          "x=y=INT_MIN with zero size is accepted");

    if (failures != 0) {
        std::cerr << failures << " texture-overlay geometry test(s) failed\n";
        return 1;
    }
    std::cout << "texture-overlay geometry tests passed\n";
    return 0;
}
