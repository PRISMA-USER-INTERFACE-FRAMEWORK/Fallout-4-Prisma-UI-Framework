#include "PluginRuntimePolicy.h"

#include <cstdlib>
#include <iostream>

namespace {

void Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

}  // namespace

int main() {
    using PrismaUI::PluginRuntimePolicy::IsOgOrAeBuild;

    static_assert(IsOgOrAeBuild(1, 10, 163));
    static_assert(IsOgOrAeBuild(1, 11, 137));
    static_assert(IsOgOrAeBuild(1, 11, 240));
    static_assert(!IsOgOrAeBuild(1, 10, 984));

    Check(IsOgOrAeBuild(1, 10, 163), "OG 1.10.163 loads");
    Check(IsOgOrAeBuild(1, 11, 137), "AE 1.11.137 keeps loading (rule unchanged)");
    Check(IsOgOrAeBuild(1, 11, 240), "AE 1.11.240 loads");
    Check(IsOgOrAeBuild(1, 11, 241), "later AE builds keep loading (rule unchanged)");
    Check(!IsOgOrAeBuild(1, 10, 984), "NG 1.10.984 fails closed");
    Check(!IsOgOrAeBuild(1, 10, 980), "NG 1.10.980 is rejected");
    Check(!IsOgOrAeBuild(1, 10, 162), "other 1.10.x builds are rejected");
    Check(!IsOgOrAeBuild(1, 11, 136), "AE below 1.11.137 is rejected");
    Check(!IsOgOrAeBuild(1, 2, 72), "VR 1.2.72 is a separate provider");
    Check(!IsOgOrAeBuild(2, 10, 163), "other majors are rejected");
    Check(!IsOgOrAeBuild(0, 0, 0), "an unset version is rejected");

    std::cout << "plugin_runtime_policy_tests ok\n";
    return 0;
}
