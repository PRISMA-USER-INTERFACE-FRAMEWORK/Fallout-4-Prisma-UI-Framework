#include "RuntimeSupportPolicy.h"

#include <iostream>

namespace {
int failures = 0;
void Check(bool value, const char* message) {
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}
}  // namespace

int main() {
    using PrismaUI::RuntimeSupportPolicy::IsSupportedGameVersion;

    Check(IsSupportedGameVersion(1, 10, 163),
          "OG 1.10.163 is supported");
    Check(!IsSupportedGameVersion(1, 10, 162) && !IsSupportedGameVersion(1, 10, 164),
          "only the supported OG patch is accepted");
    Check(!IsSupportedGameVersion(1, 10, 980),
          "intermediate NG 1.10.980 is rejected");
    Check(IsSupportedGameVersion(1, 10, 984),
          "NG 1.10.984 is admitted");
    Check(!IsSupportedGameVersion(1, 10, 983) && !IsSupportedGameVersion(1, 10, 985),
          "intermediate NG 1.10.983 and 1.10.985 are rejected");
    Check(!IsSupportedGameVersion(1, 11, 136),
          "AE versions below the minimum are rejected");
    Check(IsSupportedGameVersion(1, 11, 137),
          "the first AE runtime is supported");
    Check(IsSupportedGameVersion(1, 11, 240),
          "AE 1.11.240 is accepted");
    Check(IsSupportedGameVersion(1, 11, 241),
          "a future AE runtime is supported without a source update");
    Check(IsSupportedGameVersion(1, 12, 0),
          "future 1.x minor versions remain eligible under the forward AE policy");
    Check(!IsSupportedGameVersion(0, 12, 0) && !IsSupportedGameVersion(2, 11, 137),
          "other major versions are rejected");

    if (failures != 0) {
        std::cerr << failures << " HUD address-lookup policy test(s) failed\n";
        return 1;
    }
    std::cout << "HUD address-lookup policy tests passed\n";
    return 0;
}
