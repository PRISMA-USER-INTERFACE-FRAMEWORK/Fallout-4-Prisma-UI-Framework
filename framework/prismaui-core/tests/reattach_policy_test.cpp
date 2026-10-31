#include "Hooks/ReattachPolicy.h"

#include <cstdio>

using namespace PrismaUI::Hooks;

static int g_failures = 0;
static void expect(bool cond, const char* what) {
    std::printf("  [%s] %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) ++g_failures;
}

using Forwarder = void (*)();
static int g_forwardCalls = 0;
static void OriginalPresent() { ++g_forwardCalls; }
static void RelayPresentA() {
    PrismaUI::Hooks::SelectHookForwarder(HookInstallStrategy::SwapchainChain,
                                         static_cast<Forwarder>(&OriginalPresent),
                                         static_cast<Forwarder>(nullptr))();
}
static void ForeignPresent() { RelayPresentA(); }
static void ActivePresentB() {
    PrismaUI::Hooks::SelectHookForwarder(HookInstallStrategy::SwapchainChain,
                                         static_cast<Forwarder>(&ForeignPresent),
                                         static_cast<Forwarder>(nullptr))();
}

int main() {
    g_forwardCalls = 0;
    ActivePresentB();
    expect(g_forwardCalls == 1, "active B forwards through foreign hook and relay A to the original Present");
    expect(SelectHookForwarder(HookInstallStrategy::DirectMinHook, static_cast<Forwarder>(nullptr),
                               static_cast<Forwarder>(&OriginalPresent)) == &OriginalPresent,
           "direct strategy uses the MinHook trampoline");

    {
        HookInstallObservation clean{true, true, true, true};
        HookInstallObservation enb{true, true, false, true};
        HookInstallObservation malformed{true, true, false, false};
        HookInstallObservation unreadable{false, true, false, true};
        HookInstallObservation foreignClean{true, false, true, true};
        HookInstallObservation foreignJmp{true, false, false, true};
        HookInstallObservation foreignMalformed{true, false, false, false};
        HookInstallObservation selfOwned{true, false, true, false};
        struct StrategyCase {
            const char* name;
            HookInstallObservation present;
            HookInstallObservation resize;
            HookInstallStrategy expected;
        };
        const StrategyCase cases[] = {
            {"recognized clean pair", clean, clean, HookInstallStrategy::DirectMinHook},
            {"recognized Present plus foreign clean Resize", clean, foreignClean, HookInstallStrategy::SwapchainChain},
            {"foreign clean Present plus recognized Resize", foreignClean, clean, HookInstallStrategy::SwapchainChain},
            {"recognized Present plus foreign JMP Resize", clean, foreignJmp, HookInstallStrategy::SwapchainChain},
            {"foreign JMP Present plus recognized Resize", foreignJmp, clean, HookInstallStrategy::SwapchainChain},
            {"foreign clean pair", foreignClean, foreignClean, HookInstallStrategy::SwapchainChain},
            {"foreign decoded JMP pair", foreignJmp, foreignJmp, HookInstallStrategy::SwapchainChain},
            {"ENB Present plus foreign clean Resize", enb, foreignClean, HookInstallStrategy::SwapchainChain},
            {"foreign clean Present plus ENB Resize", foreignClean, enb, HookInstallStrategy::SwapchainChain},
            {"ENB Present plus foreign JMP Resize", enb, foreignJmp, HookInstallStrategy::SwapchainChain},
            {"foreign JMP Present plus ENB Resize", foreignJmp, enb, HookInstallStrategy::SwapchainChain},
            {"unreadable Present plus foreign Resize", unreadable, foreignClean, HookInstallStrategy::Reject},
            {"foreign Present plus unreadable Resize", foreignClean, unreadable, HookInstallStrategy::Reject},
            {"malformed Present plus foreign Resize", malformed, foreignClean, HookInstallStrategy::Reject},
            {"foreign Present plus malformed Resize", foreignClean, malformed, HookInstallStrategy::Reject},
            {"foreign malformed Present plus foreign JMP Resize", foreignMalformed, foreignJmp, HookInstallStrategy::Reject},
            {"foreign JMP Present plus foreign malformed Resize", foreignJmp, foreignMalformed, HookInstallStrategy::Reject},
            {"self-owned Present plus foreign Resize", selfOwned, foreignClean, HookInstallStrategy::Reject},
            {"foreign Present plus self-owned Resize", foreignClean, selfOwned, HookInstallStrategy::Reject},
            {"self-owned Present plus ENB Resize", selfOwned, enb, HookInstallStrategy::Reject},
            {"ENB Present plus self-owned Resize", enb, selfOwned, HookInstallStrategy::Reject},
            {"self-owned Present and Resize pair", selfOwned, selfOwned, HookInstallStrategy::Reject},
        };
        for (const auto& test : cases)
            expect(ChooseHookInstallStrategy(test.present, test.resize) == test.expected, test.name);

        struct EnbCase {
            bool present;
            bool resize;
            bool sameModule;
            bool expected;
            const char* name;
        };
        const EnbCase enbCases[] = {
            {false, false, false, true, "no ENB pair chains"},
            {true, false, false, true, "ENB Present plus foreign Resize chains"},
            {false, true, false, true, "foreign Present plus ENB Resize chains"},
            {true, true, true, true, "same ENB pair chains"},
            {true, true, false, false, "conflicting ENB pair rejects"},
        };
        for (const auto& test : enbCases)
            expect(IsChainEnbPairCompatible(test.present, test.resize, test.sameModule) == test.expected,
                   test.name);
    }

    constexpr uintptr_t PRESENT_A = 0xD00;
    constexpr uintptr_t PRESENT_B = 0xD10;
    constexpr uintptr_t RESIZE_A = 0xE00;
    constexpr uintptr_t RESIZE_B = 0xE10;
    constexpr uintptr_t HOOKED = 0x7000'1000;
    constexpr uintptr_t NEW = 0x1000'2000;
    constexpr uintptr_t SWAPCHAIN_A = 0x2000'3000;
    constexpr uintptr_t SWAPCHAIN_B = 0x2000'3010;
    HookCoverage cov;
    cov.ourDetourA = PRESENT_A;
    cov.ourDetourB = PRESENT_B;
    cov.ourResizeDetourA = RESIZE_A;
    cov.ourResizeDetourB = RESIZE_B;
    cov.hookedTarget = HOOKED;
    cov.generation = 3;
    HookCoverage chainCov = cov;
    chainCov.coveredSwapChain = SWAPCHAIN_A;
    chainCov.strategy = HookInstallStrategy::SwapchainChain;
    chainCov.chainOwned = true;
    chainCov.activePresentDetour = PRESENT_A;
    chainCov.activeResizeDetour = RESIZE_A;
    chainCov.activePresentForwardable = true;
    chainCov.activeResizeForwardable = true;
    constexpr int64_t RL = 1000;

    const auto presentDetours = SelfDetours(cov, false);
    const auto resizeDetours = SelfDetours(cov, true);
    expect(presentDetours.primary == PRESENT_A && presentDetours.alternate == PRESENT_B,
           "Present classification uses Present A/B identities");
    expect(resizeDetours.primary == RESIZE_A && resizeDetours.alternate == RESIZE_B,
           "ResizeBuffers classification uses Resize A/B identities");
    expect(resizeDetours.primary != presentDetours.primary && resizeDetours.alternate != presentDetours.alternate,
           "ResizeBuffers self identities are not borrowed from Present");

    struct RelayCase {
        bool isForwardRelay;
        bool expectedReusable;
        const char* name;
    };
    const RelayCase relayCases[] = {
        {false, true, "free hook slot remains reusable"},
        {true, false, "ForwardRelay hook slot is permanently non-reusable"},
    };
    for (const auto& test : relayCases)
        expect(RelaySlotReusable(test.isForwardRelay) == test.expectedReusable, test.name);

    struct ForwarderCase {
        bool resize;
        uintptr_t target;
        uintptr_t jumpTarget;
        uintptr_t liveSwapChain;
        bool expected;
        const char* name;
    };
    const ForwarderCase forwarderCases[] = {
        {false, PRESENT_A, 0, SWAPCHAIN_A, true, "active Present detour is a verified predecessor"},
        {false, 0, PRESENT_A, SWAPCHAIN_A, true, "Present JMP into active detour is a verified predecessor"},
        {true, RESIZE_A, 0, SWAPCHAIN_A, true, "active ResizeBuffers detour is a verified predecessor"},
        {true, 0, RESIZE_A, SWAPCHAIN_A, true, "ResizeBuffers JMP into active detour is a verified predecessor"},
        {true, PRESENT_A, 0, SWAPCHAIN_A, false, "Present identity is not a ResizeBuffers predecessor"},
        {false, RESIZE_A, 0, SWAPCHAIN_A, false, "ResizeBuffers identity is not a Present predecessor"},
        {true, RESIZE_A, 0, SWAPCHAIN_B, false, "verified predecessor on a replacement swapchain is rejected"},
    };
    for (const auto& test : forwarderCases)
        expect(CanUseActiveForwarder(chainCov, test.resize, test.liveSwapChain, test.target, test.jumpTarget) ==
                   test.expected,
               test.name);

    {
        void* liveSlots[14] = {};
        liveSlots[8] = reinterpret_cast<void*>(0x9000'1000);
        liveSlots[13] = reinterpret_cast<void*>(0x9000'2000);
        void* otherSlots[14] = {};
        void* originalPresent = reinterpret_cast<void*>(0xA000'1000);
        void* originalResize = reinterpret_cast<void*>(0xA000'2000);
        const auto chained = SelectChainSlotOriginals(liveSlots, liveSlots, originalPresent, originalResize);
        expect(chained.present == originalPresent && chained.resize == originalResize,
               "a live shadow array resolves to the chain originals, not the slots it exposes");
        const auto foreign = SelectChainSlotOriginals(liveSlots, otherSlots, originalPresent, originalResize);
        expect(foreign.present == nullptr && foreign.resize == nullptr,
               "a live array that is not the shadow keeps the live slot pointers");
        const auto missing = SelectChainSlotOriginals(liveSlots, liveSlots, originalPresent, nullptr);
        expect(missing.present == nullptr && missing.resize == nullptr,
               "a chain slot without both stored originals resolves to nothing");
        const auto unreadable = SelectChainSlotOriginals(nullptr, liveSlots, originalPresent, originalResize);
        expect(unreadable.present == nullptr && unreadable.resize == nullptr,
               "an unreadable live dispatch table resolves to nothing");
    }

    auto base = [] {
        ReattachInputs i;
        i.presentStale = true;
        i.liveTargetReadable = true;
        i.resizeTargetSafe = true;
        i.liveTargetStable = true;
        i.nowMs = 100000;
        return i;
    };

    { auto i = base(); i.presentStale = false; i.liveTarget = NEW;
      expect(ShouldReattach(i, cov, RL).action == ReattachAction::None, "present fresh -> None"); }
    { auto i = base(); i.minimized = true; i.liveTarget = NEW;
      expect(ShouldReattach(i, cov, RL).action == ReattachAction::None, "minimized -> None"); }
    { auto i = base(); i.liveTargetReadable = false;
      expect(ShouldReattach(i, cov, RL).action == ReattachAction::DiagnoseOnly, "unreadable -> DiagnoseOnly"); }
    { auto i = base(); i.liveTarget = PRESENT_A;
      expect(ShouldReattach(i, cov, RL).action == ReattachAction::None, "detour A -> None"); }
    { auto i = base(); i.liveTarget = PRESENT_B;
      expect(ShouldReattach(i, cov, RL).action == ReattachAction::None, "detour B -> None"); }
    { auto i = base(); i.liveTarget = HOOKED; i.liveTargetJumpsToUs = true;
      expect(ShouldReattach(i, cov, RL).action == ReattachAction::DiagnoseOnly,
             "same target, our JMP still there -> DiagnoseOnly"); }
    { auto i = base(); i.liveTarget = HOOKED; i.liveTargetJumpsToUs = false; i.liveTargetStable = false;
      expect(ShouldReattach(i, cov, RL).action == ReattachAction::DiagnoseOnly,
             "same target, stolen prologue, unstable -> DiagnoseOnly"); }
    { auto i = base(); i.liveTarget = HOOKED; i.liveTargetJumpsToUs = false; i.lastAttemptMs = 99500;
      expect(ShouldReattach(i, cov, RL).action == ReattachAction::None,
             "same target stolen prologue but rate-limited -> None"); }
    { auto i = base(); i.liveTarget = HOOKED; i.liveTargetJumpsToUs = false;
      expect(ShouldReattach(i, cov, RL).action == ReattachAction::DiagnoseOnly,
             "same target, foreign prologue, stable -> DiagnoseOnly"); }
    { auto i = base(); i.liveTarget = NEW; i.liveTargetStable = false;
      expect(ShouldReattach(i, cov, RL).action == ReattachAction::DiagnoseOnly, "unstable -> DiagnoseOnly"); }
    { auto i = base(); i.liveTarget = NEW; i.resizeTargetSafe = false;
      expect(ShouldReattach(i, cov, RL).action == ReattachAction::DiagnoseOnly,
             "unsafe ResizeBuffers target -> DiagnoseOnly"); }
    { auto i = base(); i.liveTarget = NEW; i.lastAttemptMs = 99500;
      expect(ShouldReattach(i, cov, RL).action == ReattachAction::None, "recent failed/success attempt -> None"); }
    { auto i = base(); i.liveTarget = NEW; i.lastAttemptMs = 98000;
      expect(ShouldReattach(i, cov, RL).action == ReattachAction::Reattach, "stable moved target -> Reattach"); }

    { auto i = base(); i.liveTarget = HOOKED; i.liveSwapChain = SWAPCHAIN_B;
      expect(ShouldReattach(i, chainCov, RL).action == ReattachAction::Reattach,
             "same Present target on a new swapchain -> Reattach"); }
    { auto i = base(); i.liveTarget = HOOKED; i.liveSwapChain = 0;
      expect(ShouldReattach(i, chainCov, RL).action == ReattachAction::DiagnoseOnly,
             "new swapchain without identity -> DiagnoseOnly"); }
    { auto i = base(); i.liveTarget = HOOKED; i.liveSwapChain = SWAPCHAIN_B; i.liveTargetStable = false;
      expect(ShouldReattach(i, chainCov, RL).action == ReattachAction::DiagnoseOnly,
             "new swapchain, unstable -> DiagnoseOnly"); }
    { auto i = base(); i.liveTarget = HOOKED; i.liveSwapChain = SWAPCHAIN_B; i.resizeTargetSafe = false;
      expect(ShouldReattach(i, chainCov, RL).action == ReattachAction::DiagnoseOnly,
             "new swapchain, unsafe ResizeBuffers -> DiagnoseOnly"); }
    { auto i = base(); i.liveTarget = HOOKED; i.liveSwapChain = SWAPCHAIN_B; i.lastAttemptMs = 99500;
      expect(ShouldReattach(i, chainCov, RL).action == ReattachAction::None,
             "new swapchain, rate-limited -> None"); }
    { auto i = base(); i.liveTarget = HOOKED; i.liveSwapChain = SWAPCHAIN_A;
      chainCov.chainOwned = false;
      expect(ShouldReattach(i, chainCov, RL).action == ReattachAction::Reattach,
             "same swapchain with lost chain ownership -> Reattach");
      chainCov.chainOwned = true;
    }
    { auto i = base(); i.liveTarget = NEW; i.liveSwapChain = SWAPCHAIN_A; i.resizeTargetSafe = false;
      chainCov.chainOwned = false;
      expect(ShouldReattach(i, chainCov, RL).action == ReattachAction::Reattach,
             "known active Resize relay permits a Present-only chain takeover");
      chainCov.chainOwned = true;
    }
    { auto i = base(); i.liveTarget = PRESENT_A; i.liveSwapChain = SWAPCHAIN_A;
      chainCov.chainOwned = false;
      expect(ShouldReattach(i, chainCov, RL).action == ReattachAction::None,
             "lost chain with Prisma Present still live -> None");
      chainCov.chainOwned = true;
    }

    {
        TrampolineQuarantine fresh;
        expect(fresh.Empty() && DecideHookEntry(fresh, 0x1000, 0) == HookEntryAction::Create,
               "an unknown target is created through MinHook");
        expect(fresh.Record(0x1000, 0x9000, 0) == QuarantineAdd::Recorded, "a created hook is quarantined");
        expect(fresh.Record(0x1000, 0x9000, 0) == QuarantineAdd::AlreadyRecorded,
               "a target is only ever quarantined once");
        expect(fresh.Find(0x1000) == 0 && fresh.At(0)->trampoline == 0x9000 && fresh.At(0)->slot == 0 &&
               !fresh.At(0)->enabled, "the quarantined trampoline and its owning slot are recorded");
        expect(DecideHookEntry(fresh, 0x1000, 0) == HookEntryAction::Reenable,
               "a disabled entry MinHook still owns is re-enabled, never re-created");
        expect(DecideHookEntry(fresh, 0x1000, 1) == HookEntryAction::Refuse,
               "an undrained entry owned by the other slot is refused: MinHook cannot retarget a detour");
        expect(fresh.MarkEnabled(0x1000, true) && fresh.At(0)->enabled, "the MinHook enable state is tracked");
        expect(!fresh.MarkEnabled(0x2000, true), "an unknown target is not marked enabled");
        expect(fresh.Forget(0x1000) && fresh.Empty(), "an entry MinHook already dropped may be forgotten");
        expect(fresh.Find(0) == -1 && fresh.At(-1) == nullptr && fresh.At(99) == nullptr,
               "out-of-range quarantine lookups are null");

        TrampolineQuarantine bounded;
        for (int i = 0; i < kQuarantineCapacity; ++i)
            bounded.Record(0x3000 + static_cast<uintptr_t>(i), 0x4000 + static_cast<uintptr_t>(i), i % 2);
        expect(bounded.Full() && bounded.Size() == kQuarantineCapacity, "the quarantine is bounded");
        expect(bounded.Record(0x9000, 0x9001, 0) == QuarantineAdd::Full,
               "a full quarantine refuses to track another trampoline");
        expect(bounded.Record(0, 0x9000, 0) != QuarantineAdd::Recorded, "a null target is never recorded");
        expect(DecideHookEntry(bounded, 0x9000, 0) == HookEntryAction::Refuse,
               "a full quarantine refuses new targets");
        expect(DecideHookEntry(bounded, 0x3001, 1) == HookEntryAction::Reenable,
               "a full quarantine still re-enables a known target");
        expect(DecideHookEntry(bounded, 0x3000, 1) == HookEntryAction::Refuse,
               "a bounded record still belongs to exactly one slot");
    }

    {
        // Mirrors the slot handoff in D3DHooks::Reattach: each reattach requests activeSlot ^ 1, and the
        // retired slot's MinHook entry stays disabled and drained (RemoveRetiredSlot).
        TrampolineQuarantine q;
        constexpr uintptr_t X = 0x5000, tA = 0xA000, tB = 0xB000;
        int active = 0;
        const auto retireEntry = [&](uintptr_t target) {
            q.MarkEnabled(target, false);
            q.MarkDrained(target, true);
        };

        expect(q.Record(X, tA, active) == QuarantineAdd::Recorded && q.MarkEnabled(X, true),
               "regression: direct install on X at slot 0");
        active ^= 1;
        retireEntry(X);
        expect(DecideHookEntry(q, X, 0) == HookEntryAction::Reenable,
               "regression: a retired entry stays with its own slot until the other slot needs it");
        active ^= 1;
        expect(DecideHookEntry(q, X, active ^ 1) == HookEntryAction::Recreate,
               "regression: direct reattach on X after two chain reattaches recreates the drained entry");
        q.Forget(X);
        expect(q.Record(X, tB, 1) == QuarantineAdd::Recorded && q.Find(X) == 0 && q.At(0)->slot == 1 &&
               q.At(0)->trampoline == tB, "regression: the re-created entry is owned by slot 1 with its new trampoline");
        expect(DecideHookEntry(q, X, 1) == HookEntryAction::Reenable,
               "regression: the re-created entry is re-enabled for its slot, never refused");

        TrampolineQuarantine undrained;
        undrained.Record(0x6000, 0xC000, 0);
        undrained.MarkEnabled(0x6000, false);
        expect(DecideHookEntry(undrained, 0x6000, 1) == HookEntryAction::Refuse,
               "a disabled other-slot entry whose retirement has not drained is refused");

        TrampolineQuarantine reenabled;
        reenabled.Record(0x7000, 0xD000, 0);
        reenabled.MarkDrained(0x7000, true);
        expect(reenabled.MarkEnabled(0x7000, true) && !reenabled.At(0)->drained,
               "re-enabling an entry clears its drained mark");
        expect(DecideHookEntry(reenabled, 0x7000, 1) == HookEntryAction::Refuse,
               "an enabled other-slot entry is refused even after a drain was recorded earlier");
        expect(!reenabled.MarkDrained(0x8000, true), "an unknown target is never marked drained");

        TrampolineQuarantine fullDrained;
        for (int i = 0; i < kQuarantineCapacity; ++i) {
            const auto target = 0xE000 + static_cast<uintptr_t>(i);
            fullDrained.Record(target, 0xF000 + static_cast<uintptr_t>(i), 0);
            fullDrained.MarkEnabled(target, false);
            fullDrained.MarkDrained(target, true);
        }
        expect(fullDrained.Full() && DecideHookEntry(fullDrained, 0xE000, 1) == HookEntryAction::Recreate,
               "a recreate on a full quarantine is allowed because the old entry is forgotten first");
        expect(DecideHookEntry(fullDrained, 0x9999, 1) == HookEntryAction::Refuse,
               "a full quarantine still refuses a brand-new target");
    }

    if (g_failures == 0) {
        std::printf("\nAll reattach-policy assertions passed.\n");
        return 0;
    }
    std::printf("\n%d assertion(s) FAILED.\n", g_failures);
    return 1;
}
