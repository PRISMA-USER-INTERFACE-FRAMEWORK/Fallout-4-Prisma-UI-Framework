#include <cstdlib>
#include <iostream>
#include <string>

#include "Engine/Interface3DOffscreenSlotPolicy.h"

namespace {

    using namespace PrismaUI::Engine::Interface3DOffscreenSlotPolicy;

    // 0x25 is the slot value the OG/NG/AE workshop builder stores at the renderer offscreen slot offset, 0x70..0x8F the VR picker range; source: ReverseEng/Fallout4_Runtime_Research@030de195 interface3d-srv-vr-q1q6, AE from #905 comment 6300072506
    constexpr std::int32_t kOgNgStoredSlot = 0x25;
    constexpr std::int32_t kVrLowestSlot = 0x70;
    constexpr std::int32_t kVrHighestSlot = 0x8F;

    void Check(bool condition, const char* message) {
        if (!condition) {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(1);
        }
    }

}

int main() {
    static_assert(SlotOffsetKnown(Layout::OG));
    static_assert(SlotOffsetKnown(Layout::NG));
    static_assert(SlotOffsetKnown(Layout::VR));
    static_assert(SlotOffsetKnown(Layout::AE));

    Check(SlotOffsetKnown(Layout::OG), "the OG slot offset is known");
    Check(SlotOffsetKnown(Layout::NG), "the NG slot offset is known");
    Check(SlotOffsetKnown(Layout::VR), "the VR slot offset is known");
    Check(SlotOffsetKnown(Layout::AE), "the AE slot offset is known from #905 comment 6300072506");

    Check(Classify(Layout::OG, kOgNgStoredSlot) == SlotState::Valid, "OG with a stored slot is valid");
    Check(Classify(Layout::NG, kOgNgStoredSlot) == SlotState::Valid, "NG with a stored slot is valid");
    Check(Classify(Layout::OG, kNoSlot) == SlotState::NotYetValid, "OG at -1 is not yet valid");
    Check(Classify(Layout::NG, kNoSlot) == SlotState::NotYetValid, "NG at -1 is not yet valid");
    Check(Classify(Layout::OG, -2) == SlotState::NotYetValid, "any other negative value is not yet valid either");

    Check(Classify(Layout::AE, kOgNgStoredSlot) == SlotState::Valid, "AE with the stored slot 0x25 is valid");
    Check(Classify(Layout::AE, kNoSlot) == SlotState::NotYetValid, "AE at -1 is not yet valid");
    Check(Classify(Layout::AE, 0) == SlotState::Valid, "AE accepts a runtime value that is not -1");

    static_assert(FlatRuntimeLayout(true, false) == Layout::OG);
    static_assert(FlatRuntimeLayout(false, true) == Layout::NG);
    static_assert(FlatRuntimeLayout(false, false) == Layout::AE);

    Check(FlatRuntimeLayout(true, false) == Layout::OG, "OG maps to the OG layout");
    Check(FlatRuntimeLayout(false, true) == Layout::NG, "NG maps to the NG layout");
    Check(FlatRuntimeLayout(false, false) == Layout::AE, "AE maps to the AE layout");

    Check(Classify(FlatRuntimeLayout(true, false), kOgNgStoredSlot) == SlotState::Valid,
          "OG reads the stored slot");
    Check(Classify(FlatRuntimeLayout(false, true), kOgNgStoredSlot) == SlotState::Valid,
          "NG reads the stored slot");
    Check(Classify(FlatRuntimeLayout(false, false), kOgNgStoredSlot) == SlotState::Valid,
          "AE reads the stored slot");

    Check(Classify(Layout::VR, kVrLowestSlot) == SlotState::Valid, "VR accepts the lowest picked slot");
    Check(Classify(Layout::VR, kVrHighestSlot) == SlotState::Valid, "VR accepts the highest picked slot");
    Check(Classify(Layout::VR, 0) == SlotState::Valid, "VR accepts a runtime value that is not -1, no constant is assumed");
    Check(Classify(Layout::VR, kNoSlot) == SlotState::NotYetValid, "VR at -1 is not yet valid, the slot is picked after Enable");

    Check(kNoSlot == -1, "the sentinel is -1");

    Check(std::string(Name(SlotState::Valid)) == "valid", "Valid names itself");
    Check(std::string(Name(SlotState::NotYetValid)) == "not-yet-valid", "NotYetValid names itself");
    Check(std::string(Name(SlotState::UnknownLayout)) == "unknown-layout", "UnknownLayout names itself");

    std::cout << "interface3d_offscreen_slot_policy_tests ok\n";
    return 0;
}
