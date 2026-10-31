#include "Engine/ViewNetworkPolicy.h"

#include <cstdint>
#include <iostream>
#include <string>

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
    namespace Policy = PrismaUI::Engine::ViewNetworkPolicy;

    constexpr std::uint64_t kViewA = 0x1001;
    constexpr std::uint64_t kViewB = 0x1002;

    Policy::Forget(kViewA);
    Policy::Forget(kViewB);

    Check(Policy::Get(kViewA) == Policy::kRemoteNoFile,
          "a new view starts at the fail-closed RemoteNoFile policy");
    Check(Policy::GetOriginHost(kViewA).empty(),
          "a new view has no origin owner until creation records one");

    Policy::Set(kViewA, Policy::kLocalOnly);
    Check(Policy::Get(kViewA) == Policy::kLocalOnly,
          "LocalOnly takes effect for the selected view");
    Check(Policy::Get(kViewB) == Policy::kRemoteNoFile,
          "changing one view does not change another view");

    // This is the regression contract from PrismaPrivate #31: native policy changes must be
    // reversible immediately, without a page reload or an injected JavaScript wrapper lingering.
    Policy::Set(kViewA, Policy::kUnrestricted);
    Check(Policy::Get(kViewA) == Policy::kUnrestricted,
          "unlocking a LocalOnly view is immediately reflected by Get");

    Policy::Set(kViewA, Policy::kRemoteNoFile);
    Check(Policy::Get(kViewA) == Policy::kRemoteNoFile,
          "a view can be locked to RemoteNoFile after being unrestricted");

    Policy::Set(kViewA, -1);
    Check(Policy::Get(kViewA) == Policy::kRemoteNoFile,
          "an invalid negative policy cannot replace the current policy");
    Policy::Set(kViewA, Policy::kRemoteNoFile + 1);
    Check(Policy::Get(kViewA) == Policy::kRemoteNoFile,
          "an out-of-range policy cannot replace the current policy");
    Policy::Set(0, Policy::kLocalOnly);
    Check(Policy::Get(0) == Policy::kRemoteNoFile,
          "view zero cannot acquire mutable policy state");

    Policy::SetOriginHost(kViewA, "VIEW-AbCdEf0123456789");
    Check(Policy::GetOriginHost(kViewA) == "view-abcdef0123456789",
          "origin ownership is normalized to lowercase");
    Policy::SetOriginHost(kViewA, "view-replacement");
    Check(Policy::GetOriginHost(kViewA) == "view-abcdef0123456789",
          "origin ownership is immutable after the view is created");
    Policy::SetOriginHost(kViewB, "");
    Check(Policy::GetOriginHost(kViewB).empty(),
          "an empty origin host is rejected");

    Policy::Set(kViewB, Policy::kLocalOnly);
    Policy::SetOriginHost(kViewB, "VIEW-B");
    Check(Policy::Get(kViewB) == Policy::kLocalOnly,
          "a second view retains independent policy state");
    Check(Policy::GetOriginHost(kViewB) == "view-b",
          "a second view retains independent origin ownership");

    Policy::Forget(kViewA);
    Check(Policy::Get(kViewA) == Policy::kRemoteNoFile,
          "destroying a view resets it to the fail-closed default");
    Check(Policy::GetOriginHost(kViewA).empty(),
          "destroying a view removes its origin ownership");
    Check(Policy::Get(kViewB) == Policy::kLocalOnly,
          "destroying one view does not erase another view");

    Policy::Forget(kViewB);

    if (failures != 0) {
        std::cerr << failures << " view-network-policy test(s) failed\n";
        return 1;
    }

    std::cout << "view network policy tests passed\n";
    return 0;
}
