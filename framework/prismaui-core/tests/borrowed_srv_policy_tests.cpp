#include <cstdlib>
#include <iostream>

#include "PrismaUI/BorrowedSrvPolicy.h"

namespace {

    void Check(bool condition, const char* message) {
        if (!condition) {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(1);
        }
    }

}

int main() {
    using PrismaUI::BorrowedSrvPolicy::Decide;
    using PrismaUI::BorrowedSrvPolicy::Decision;
    using PrismaUI::BorrowedSrvPolicy::Owner;

    static_assert(Decide(Owner::PrismaOwned, true) == Decision::Retain);

    Check(Decide(Owner::PrismaOwned, true) == Decision::Retain, "a Prisma-owned SRV on the same device is retained");
    Check(Decide(Owner::PrismaOwned, false) == Decision::Reject, "a Prisma-owned SRV from another device is rejected");
    Check(Decide(Owner::EngineOwned, true) == Decision::Reject, "an engine-owned SRV is never retained");
    Check(Decide(Owner::EngineOwned, false) == Decision::Reject, "an engine-owned SRV from another device is rejected");

    std::cout << "borrowed_srv_policy_tests ok\n";
    return 0;
}
