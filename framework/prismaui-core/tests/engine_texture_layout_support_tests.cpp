#include "Engine/EngineTextureLayoutSupport.h"

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
    using PrismaUI::Engine::IsVerifiedTextureLayoutBuild;

    Check(IsVerifiedTextureLayoutBuild(1, 10, 163), "OG 1.10.163 is in the verified set");
    Check(IsVerifiedTextureLayoutBuild(1, 11, 240), "AE 1.11.240 is in the verified set");
    Check(!IsVerifiedTextureLayoutBuild(1, 10, 984), "NG 1.10.984 is not in the verified set");
    Check(!IsVerifiedTextureLayoutBuild(1, 10, 980), "NG 1.10.980 is rejected");
    Check(!IsVerifiedTextureLayoutBuild(1, 11, 221), "AE 1.11.221 is not in the verified set");
    Check(!IsVerifiedTextureLayoutBuild(1, 11, 241), "AE 1.11.241 is not in the verified set");
    Check(!IsVerifiedTextureLayoutBuild(1, 11, 137), "AE 1.11.137 is not in the verified set");
    Check(!IsVerifiedTextureLayoutBuild(1, 2, 72), "VR 1.2.72 is not in the verified set");
    Check(!IsVerifiedTextureLayoutBuild(2, 11, 240), "other majors are rejected");

    std::cout << "engine_texture_layout_support_tests ok\n";
    return 0;
}
