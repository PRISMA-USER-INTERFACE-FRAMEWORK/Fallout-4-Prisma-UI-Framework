#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>

#include "API/MeshBindingSrvPolicy.h"

namespace {

    void Check(bool condition, const char* message) {
        if (!condition) {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(1);
        }
    }

    struct Entry {
        std::uint64_t view;
        const void* texture;
    };

    std::uint64_t FindBoundOwner(const Entry* entries, std::size_t count, std::uint64_t candidate,
                                 const void* texture) {
        for (std::size_t i = 0; i < count; ++i) {
            if (PrismaUI::MeshBinding::SrvPolicy::ConflictsWithOtherView(
                    entries[i].view, candidate, entries[i].texture, texture)) {
                return entries[i].view;
            }
        }
        return 0;
    }

}

int main() {
    using PrismaUI::MeshBinding::SrvPolicy::ConflictsWithOtherView;
    using PrismaUI::MeshBinding::SrvPolicy::DecideRestore;
    using PrismaUI::MeshBinding::SrvPolicy::Restore;

    static_assert(DecideRestore(nullptr, nullptr) == Restore::KeepReplaced);
    static_assert(!ConflictsWithOtherView(7, 8, nullptr, nullptr));

    int gameSrv = 0;
    int viewA = 0;
    int viewB = 0;
    int gameReplacement = 0;
    const void* const g = &gameSrv;
    const void* const a = &viewA;
    const void* const b = &viewB;
    const void* const r = &gameReplacement;

    Check(DecideRestore(a, a) == Restore::Original, "the slot still holds our SRV, so it is restored");
    Check(DecideRestore(g, a) == Restore::KeepReplaced, "the game replaced the slot, so it is left alone");
    Check(DecideRestore(r, a) == Restore::KeepReplaced, "another binding replaced the slot, so it is left alone");
    Check(DecideRestore(nullptr, a) == Restore::KeepReplaced, "a nulled slot is not overwritten");
    Check(DecideRestore(a, nullptr) == Restore::KeepReplaced, "a binding that never wrote has nothing to undo");

    const Entry bound[] = { { 7, g }, { 9, r } };
    Check(FindBoundOwner(bound, 2, 8, g) == 7, "a second view on a bound texture is refused, naming the owner");
    Check(FindBoundOwner(bound, 2, 7, g) == 0, "the owning view may rebind its own texture");
    Check(FindBoundOwner(bound, 2, 8, r) == 9, "each texture is tracked separately");
    Check(FindBoundOwner(bound, 2, 8, a) == 0, "an unbound texture is free");
    Check(DecideRestore(b, b) == Restore::Original, "a re-pointed slot still matches weWrote");

    std::cout << "mesh_binding_srv_policy_tests ok\n";
    return 0;
}