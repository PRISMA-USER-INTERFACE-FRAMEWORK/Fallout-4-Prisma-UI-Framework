#include "GPU/AcceleratedDeviceLifecycle.h"
#include "GPU/AcceleratedProxyOwnershipPolicy.h"
#include "Utils/SwapchainWidthPolicy.h"

#include <cstdlib>
#include <iostream>

namespace {

using PrismaUI::GPU::AcceleratedDeviceLifecycle;
using PrismaUI::GPU::AcceleratedLifecycleInputs;
using PrismaUI::GPU::AcceleratedProxyEligibilityReason;
using PrismaUI::GPU::AcceleratedProxyOwnershipClass;
using PrismaUI::GPU::AcceleratedProxyOwnershipEvidence;
using PrismaUI::GPU::DeviceIdentityMatches;
using PrismaUI::GPU::EvaluateKnownInterposerSwapchainCoherence;
using PrismaUI::GPU::EvaluateAcceleratedProxyOwnership;
using PrismaUI::GPU::KnownInterposerSwapchainEvidence;
using PrismaUI::ConflictChecker::SelectShadowSlotCount;
using PrismaUI::ConflictChecker::SwapchainInterfaceProbe;
using PrismaUI::ConflictChecker::SwapchainWidthEvidence;

void Check(bool condition, const char* message)
{
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

AcceleratedProxyOwnershipEvidence CoherentDirect()
{
    AcceleratedProxyOwnershipEvidence evidence;
    evidence.canonicalDevicePresent = true;
    evidence.immediateContextPresent = true;
    evidence.immediateContextType = true;
    evidence.contextDeviceMatchesCanonical = true;
    evidence.d3d11ModuleEvidenceKnown = true;
    evidence.proxyModuleDetected = false;
    evidence.deviceDispatchKnown = true;
    evidence.contextDispatchKnown = true;
    evidence.dispatchModulesAgree = true;
    evidence.deviceDispatchMatchesRuntime = true;
    evidence.contextDispatchMatchesRuntime = true;
    return evidence;
}

}  // namespace

int main()
{
    {
        const auto result = EvaluateAcceleratedProxyOwnership(CoherentDirect());
        Check(result.eligible, "native coherent ownership must be eligible");
        Check(result.classification == AcceleratedProxyOwnershipClass::DirectCoherent,
              "native coherent ownership must classify direct-coherent");
        Check(result.reason == AcceleratedProxyEligibilityReason::EligibleDirect,
              "native coherent ownership must have the direct eligibility reason");
    }

    {
        auto evidence = CoherentDirect();
        evidence.deviceDispatchMatchesRuntime = false;
        evidence.contextDispatchMatchesRuntime = false;
        evidence.proxyModuleDetected = false;
        const auto result = EvaluateAcceleratedProxyOwnership(evidence);
        Check(!result.eligible, "foreign coherent dispatch must remain blocked outside controlled testing");
        Check(result.classification == AcceleratedProxyOwnershipClass::ProxyCoherent,
              "foreign coherent dispatch must not classify as direct-coherent");
        Check(result.reason == AcceleratedProxyEligibilityReason::ProxyRequiresRuntimeAcceptance,
              "foreign coherent dispatch must require runtime acceptance");

        evidence.controlledProxyTest = true;
        const auto controlled = EvaluateAcceleratedProxyOwnership(evidence);
        Check(controlled.eligible, "foreign coherent dispatch may be admitted only for controlled testing");
        Check(controlled.reason == AcceleratedProxyEligibilityReason::EligibleControlledProxyTest,
              "controlled foreign dispatch eligibility must be explicit");
    }

    {
        auto evidence = CoherentDirect();
        evidence.contextDispatchMatchesRuntime = false;
        const auto result = EvaluateAcceleratedProxyOwnership(evidence);
        Check(!result.eligible, "mixed runtime and foreign dispatch must fail closed");
        Check(result.classification == AcceleratedProxyOwnershipClass::InconsistentOwnership,
              "mixed runtime and foreign dispatch must classify inconsistent");
        Check(result.reason == AcceleratedProxyEligibilityReason::DispatchOwnershipInconsistent,
              "mixed runtime and foreign dispatch must retain an exact rejection reason");
    }

    {
        auto evidence = CoherentDirect();
        evidence.proxyModuleDetected = true;
        const auto blocked = EvaluateAcceleratedProxyOwnership(evidence);
        Check(!blocked.eligible, "a coherent proxy must remain blocked outside controlled testing");
        Check(blocked.classification == AcceleratedProxyOwnershipClass::ProxyCoherent,
              "a coherent proxy must be distinguished from unknown proxy ownership");
        Check(blocked.reason == AcceleratedProxyEligibilityReason::ProxyRequiresRuntimeAcceptance,
              "a coherent proxy must state that runtime acceptance is still required");

        evidence.controlledProxyTest = true;
        const auto controlled = EvaluateAcceleratedProxyOwnership(evidence);
        Check(controlled.eligible, "the same coherent proxy evidence may be eligible for a controlled test");
        Check(controlled.reason == AcceleratedProxyEligibilityReason::EligibleControlledProxyTest,
              "controlled proxy eligibility must be explicit");
    }

    {
        auto evidence = CoherentDirect();
        evidence.proxyModuleDetected = true;
        evidence.dispatchModulesAgree = false;
        evidence.deviceDispatchMatchesRuntime = false;
        evidence.contextDispatchMatchesRuntime = true;
        evidence.knownInterposerChain = true;
        evidence.deviceDispatchIsSystem = true;
        evidence.contextDispatchIsKnownInterposer = true;
        evidence.swapchainOwnershipCoherent = true;
        const auto blocked = EvaluateAcceleratedProxyOwnership(evidence);
        Check(!blocked.eligible, "a recognized interposer chain must remain blocked outside controlled testing");
        Check(blocked.classification == AcceleratedProxyOwnershipClass::ProxyCoherent,
              "a recognized interposer chain must classify as proxy-coherent");
        Check(blocked.reason == AcceleratedProxyEligibilityReason::ProxyRequiresRuntimeAcceptance,
              "a recognized interposer chain must require runtime acceptance");

        evidence.controlledProxyTest = true;
        const auto controlled = EvaluateAcceleratedProxyOwnership(evidence);
        Check(controlled.eligible, "a coherent recognized interposer may be admitted for controlled testing");
        Check(controlled.reason == AcceleratedProxyEligibilityReason::EligibleControlledProxyTest,
              "recognized interposer eligibility must be explicitly controlled");
    }

    {
        const KnownInterposerSwapchainEvidence direct{
            true, true, false, false, false, false, false, true, false};
        Check(EvaluateKnownInterposerSwapchainCoherence(direct),
              "live matching interposer targets must prove chain coherence");

        const KnownInterposerSwapchainEvidence shadow{
            false, false, true, true, true, true, true, false, true};
        Check(EvaluateKnownInterposerSwapchainCoherence(shadow),
              "Prisma detours with matching recorded interposer targets must prove chain coherence");

        const KnownInterposerSwapchainEvidence foreignTakeover{
            false, false, false, true, true, true, true, false, true};
        Check(!EvaluateKnownInterposerSwapchainCoherence(foreignTakeover),
              "a foreign takeover of one live slot must invalidate stale chain evidence");

        const KnownInterposerSwapchainEvidence mismatchedLiveModules{
            true, true, false, false, false, false, false, false, false};
        Check(!EvaluateKnownInterposerSwapchainCoherence(mismatchedLiveModules),
              "different live interposer modules must not prove chain coherence");
    }

    {
        const auto baseOnly = SwapchainWidthEvidence{
            true, true, true, false, true, true,
            SwapchainInterfaceProbe::AliasesBase, SwapchainInterfaceProbe::AliasesBase,
            SwapchainInterfaceProbe::AliasesBase, SwapchainInterfaceProbe::AliasesBase};
        Check(SelectShadowSlotCount(baseOnly) == 18,
              "the known base-only frame-generation proxy must use the base interface width");

        const auto enbDistinct = SwapchainWidthEvidence{
            true, true, true, false, false, false,
            SwapchainInterfaceProbe::DistinctInterface, SwapchainInterfaceProbe::NotSupported,
            SwapchainInterfaceProbe::NotSupported, SwapchainInterfaceProbe::NotSupported};
        Check(SelectShadowSlotCount(enbDistinct) == 18,
              "a coherent ENB pair with distinct extended interfaces must use the base width");

        auto enbAliases = enbDistinct;
        enbAliases.swapChain4 = SwapchainInterfaceProbe::AliasesBase;
        Check(SelectShadowSlotCount(enbAliases) == 0,
              "a same-pointer ENB extended interface must leave the width unproven");

        auto mixedProxy = enbDistinct;
        mixedProxy.resizeKnownProxy = false;
        mixedProxy.unrecognizedProxy = true;
        Check(SelectShadowSlotCount(mixedProxy) == 18,
              "mixed known and foreign hooks may use a proven base interface width");

        auto mixedAliases = mixedProxy;
        mixedAliases.swapChain4 = SwapchainInterfaceProbe::AliasesBase;
        Check(SelectShadowSlotCount(mixedAliases) == 41,
              "mixed known and foreign hooks may use a proven extended interface width");

        const auto unknownProxy = SwapchainWidthEvidence{
            false, false, false, true, false, false,
            SwapchainInterfaceProbe::DistinctInterface, SwapchainInterfaceProbe::NotSupported,
            SwapchainInterfaceProbe::NotSupported, SwapchainInterfaceProbe::NotSupported};
        Check(SelectShadowSlotCount(unknownProxy) == 18,
              "a foreign hook may use a proven base interface width");

        auto unknownProbeFailure = unknownProxy;
        unknownProbeFailure.swapChain1 = SwapchainInterfaceProbe::Failed;
        Check(SelectShadowSlotCount(unknownProbeFailure) == 0,
              "a foreign hook with failed interface probing must reject the chain");

        const auto nativeSwapchain4 = SwapchainWidthEvidence{
            false, false, false, false, false, false,
            SwapchainInterfaceProbe::AliasesBase, SwapchainInterfaceProbe::NotSupported,
            SwapchainInterfaceProbe::NotSupported, SwapchainInterfaceProbe::NotSupported};
        Check(SelectShadowSlotCount(nativeSwapchain4) == 41,
              "a native swapchain with an aliased SwapChain4 interface must use 41 slots");

        const auto unrecognizedAliasedChain = SwapchainWidthEvidence{
            false, false, true, true, false, false,
            SwapchainInterfaceProbe::AliasesBase, SwapchainInterfaceProbe::AliasesBase,
            SwapchainInterfaceProbe::AliasesBase, SwapchainInterfaceProbe::AliasesBase};
        Check(SelectShadowSlotCount(unrecognizedAliasedChain) == 41,
              "an unrecognized same-module proxy with an aliased SwapChain4 interface must use 41 slots");
    }

    {
        auto evidence = CoherentDirect();
        evidence.contextDeviceMatchesCanonical = false;
        const auto result = EvaluateAcceleratedProxyOwnership(evidence);
        Check(!result.eligible, "context/device mismatch must fail closed");
        Check(result.classification == AcceleratedProxyOwnershipClass::InconsistentOwnership,
              "context/device mismatch must classify inconsistent");
        Check(result.reason == AcceleratedProxyEligibilityReason::ContextDeviceMismatch,
              "context/device mismatch must preserve an exact diagnostic reason");
    }

    {
        auto evidence = CoherentDirect();
        evidence.immediateContextType = false;
        const auto result = EvaluateAcceleratedProxyOwnership(evidence);
        Check(!result.eligible, "a deferred/non-immediate context must fail closed");
        Check(result.reason == AcceleratedProxyEligibilityReason::ContextNotImmediate,
              "non-immediate context rejection must be deterministic");
    }

    {
        Check(DeviceIdentityMatches(0xA, 0xA), "matching canonical/resource devices must pass identity proof");
        Check(!DeviceIdentityMatches(0xA, 0xB), "resource device B must be rejected against canonical device A");
        Check(!DeviceIdentityMatches(0, 0), "missing device identity must never count as ownership proof");
    }

    {
        auto evidence = CoherentDirect();
        evidence.proxyModuleDetected = true;
        evidence.deviceDispatchKnown = false;
        const auto result = EvaluateAcceleratedProxyOwnership(evidence);
        Check(!result.eligible, "a proxy with unresolved COM dispatch provenance must fail closed");
        Check(result.classification == AcceleratedProxyOwnershipClass::UnknownProxyChain,
              "unresolved proxy provenance must classify unknown");
        Check(result.reason == AcceleratedProxyEligibilityReason::ProxyDispatchEvidenceUnavailable,
              "unknown proxy provenance must have an exact diagnostic reason");
    }

    {
        auto evidence = CoherentDirect();
        evidence.proxyModuleDetected = true;
        evidence.dispatchModulesAgree = false;
        evidence.controlledProxyTest = true;
        const auto result = EvaluateAcceleratedProxyOwnership(evidence);
        Check(!result.eligible, "contradictory COM dispatch ownership cannot be overridden for testing");
        Check(result.classification == AcceleratedProxyOwnershipClass::InconsistentOwnership,
              "contradictory COM dispatch ownership must classify inconsistent");
        Check(result.reason == AcceleratedProxyEligibilityReason::DispatchOwnershipInconsistent,
              "contradictory dispatch ownership must retain an exact rejection reason");
    }

    {
        auto evidence = CoherentDirect();
        evidence.d3d11ModuleEvidenceKnown = false;
        const auto result = EvaluateAcceleratedProxyOwnership(evidence);
        Check(!result.eligible, "unknown process/system d3d11 evidence must fail closed");
        Check(result.classification == AcceleratedProxyOwnershipClass::UnknownProxyChain,
              "unknown module evidence must classify unknown");
    }

    {
        auto evidence = CoherentDirect();
        evidence.contextDispatchKnown = false;
        const auto result = EvaluateAcceleratedProxyOwnership(evidence);
        Check(!result.eligible, "unresolved native dispatch provenance must fail closed");
        Check(result.classification == AcceleratedProxyOwnershipClass::UnknownProxyChain,
              "unresolved native dispatch provenance must classify unknown");
        Check(result.reason == AcceleratedProxyEligibilityReason::ProxyDispatchEvidenceUnavailable,
              "unresolved native dispatch provenance must retain an exact reason");
    }

    {
        AcceleratedDeviceLifecycle lifecycle;
        const AcceleratedLifecycleInputs oldInputs{0x10, 0x20, 0x30};
        const AcceleratedLifecycleInputs replacement{0x10, 0x20, 0x31};
        Check(lifecycle.Observe(oldInputs) == AcceleratedDeviceLifecycle::Transition::Stable,
              "first coherent lifecycle observation establishes the epoch");
        const auto oldEpoch = lifecycle.Epoch();
        Check(lifecycle.Accepts(oldEpoch, oldInputs), "current swapchain identity must be accepted");
        Check(lifecycle.Observe(replacement) == AcceleratedDeviceLifecycle::Transition::Changed,
              "swapchain replacement must advance lifecycle identity");
        Check(lifecycle.Epoch() != oldEpoch, "swapchain replacement must advance the epoch");
        Check(!lifecycle.Accepts(oldEpoch, oldInputs), "old swapchain epoch must be rejected");
        Check(lifecycle.Accepts(lifecycle.Epoch(), replacement),
              "replacement swapchain must require and pass fresh lifecycle validation");
    }

    std::cout << "accelerated proxy ownership policy tests passed\n";
    return 0;
}
