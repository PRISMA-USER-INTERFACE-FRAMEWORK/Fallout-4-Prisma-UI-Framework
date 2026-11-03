#pragma once

#include <cstdint>
#include <string_view>

namespace PrismaUI::GPU {

enum class AcceleratedProxyOwnershipClass : std::uint8_t {
    DirectCoherent,
    ProxyCoherent,
    UnknownProxyChain,
    InconsistentOwnership,
};

enum class AcceleratedProxyEligibilityReason : std::uint8_t {
    EligibleDirect,
    EligibleControlledProxyTest,
    MissingCanonicalDevice,
    MissingImmediateContext,
    ContextNotImmediate,
    ContextDeviceMismatch,
    D3D11ModuleEvidenceUnavailable,
    ProxyDispatchEvidenceUnavailable,
    DispatchOwnershipInconsistent,
    ProxyRequiresRuntimeAcceptance,
};

struct AcceleratedProxyOwnershipEvidence {
    bool canonicalDevicePresent = false;
    bool immediateContextPresent = false;
    bool immediateContextType = false;
    bool contextDeviceMatchesCanonical = false;
    bool d3d11ModuleEvidenceKnown = false;
    bool proxyModuleDetected = false;
    bool deviceDispatchKnown = false;
    bool contextDispatchKnown = false;
    bool dispatchModulesAgree = false;
    bool deviceDispatchMatchesRuntime = false;
    bool contextDispatchMatchesRuntime = false;
    bool controlledProxyTest = false;
    bool knownInterposerChain = false;
    bool deviceDispatchIsSystem = false;
    bool contextDispatchIsKnownInterposer = false;
    bool swapchainOwnershipCoherent = false;
};

struct AcceleratedProxyOwnershipResult {
    AcceleratedProxyOwnershipClass classification = AcceleratedProxyOwnershipClass::UnknownProxyChain;
    AcceleratedProxyEligibilityReason reason = AcceleratedProxyEligibilityReason::D3D11ModuleEvidenceUnavailable;
    bool eligible = false;
};

struct KnownInterposerSwapchainEvidence {
    bool livePresentIsKnown = false;
    bool liveResizeIsKnown = false;
    bool livePresentIsPrisma = false;
    bool liveResizeIsPrisma = false;
    bool recordedPresentIsKnown = false;
    bool recordedResizeIsKnown = false;
    bool prismaOwnsCurrentChain = false;
    bool liveTargetsAgree = false;
    bool recordedTargetsAgree = false;
};

[[nodiscard]] constexpr bool EvaluateKnownInterposerSwapchainCoherence(
    const KnownInterposerSwapchainEvidence& evidence) noexcept {
    if (evidence.livePresentIsKnown && evidence.liveResizeIsKnown)
        return evidence.liveTargetsAgree;
    if (!evidence.livePresentIsPrisma || !evidence.liveResizeIsPrisma)
        return false;
    return evidence.prismaOwnsCurrentChain && evidence.recordedPresentIsKnown && evidence.recordedResizeIsKnown &&
           evidence.recordedTargetsAgree;
}

[[nodiscard]] constexpr bool DeviceIdentityMatches(std::uintptr_t canonicalDevice,
                                                   std::uintptr_t observedDevice) noexcept {
    return canonicalDevice != 0 && canonicalDevice == observedDevice;
}

[[nodiscard]] constexpr AcceleratedProxyOwnershipResult EvaluateAcceleratedProxyOwnership(
    const AcceleratedProxyOwnershipEvidence& evidence) noexcept {
    if (!evidence.canonicalDevicePresent)
        return {AcceleratedProxyOwnershipClass::InconsistentOwnership,
                AcceleratedProxyEligibilityReason::MissingCanonicalDevice, false};
    if (!evidence.immediateContextPresent)
        return {AcceleratedProxyOwnershipClass::InconsistentOwnership,
                AcceleratedProxyEligibilityReason::MissingImmediateContext, false};
    if (!evidence.immediateContextType)
        return {AcceleratedProxyOwnershipClass::InconsistentOwnership,
                AcceleratedProxyEligibilityReason::ContextNotImmediate, false};
    if (!evidence.contextDeviceMatchesCanonical)
        return {AcceleratedProxyOwnershipClass::InconsistentOwnership,
                AcceleratedProxyEligibilityReason::ContextDeviceMismatch, false};
    if (!evidence.d3d11ModuleEvidenceKnown)
        return {AcceleratedProxyOwnershipClass::UnknownProxyChain,
                AcceleratedProxyEligibilityReason::D3D11ModuleEvidenceUnavailable, false};

    if (!evidence.deviceDispatchKnown || !evidence.contextDispatchKnown)
        return {AcceleratedProxyOwnershipClass::UnknownProxyChain,
                AcceleratedProxyEligibilityReason::ProxyDispatchEvidenceUnavailable, false};

    const bool controlledInterposerOwnership =
        evidence.knownInterposerChain && evidence.proxyModuleDetected && evidence.deviceDispatchIsSystem &&
        evidence.contextDispatchIsKnownInterposer && evidence.swapchainOwnershipCoherent;

    if (evidence.deviceDispatchKnown && evidence.contextDispatchKnown && !evidence.dispatchModulesAgree &&
        !controlledInterposerOwnership)
        return {AcceleratedProxyOwnershipClass::InconsistentOwnership,
                AcceleratedProxyEligibilityReason::DispatchOwnershipInconsistent, false};

    if (evidence.deviceDispatchMatchesRuntime != evidence.contextDispatchMatchesRuntime &&
        !controlledInterposerOwnership)
        return {AcceleratedProxyOwnershipClass::InconsistentOwnership,
                AcceleratedProxyEligibilityReason::DispatchOwnershipInconsistent, false};

    if (controlledInterposerOwnership) {
        if (!evidence.controlledProxyTest)
            return {AcceleratedProxyOwnershipClass::ProxyCoherent,
                    AcceleratedProxyEligibilityReason::ProxyRequiresRuntimeAcceptance, false};
        return {AcceleratedProxyOwnershipClass::ProxyCoherent,
                AcceleratedProxyEligibilityReason::EligibleControlledProxyTest, true};
    }

    if (!evidence.proxyModuleDetected && evidence.deviceDispatchMatchesRuntime &&
        evidence.contextDispatchMatchesRuntime)
        return {AcceleratedProxyOwnershipClass::DirectCoherent,
                AcceleratedProxyEligibilityReason::EligibleDirect, true};

    if (!evidence.controlledProxyTest)
        return {AcceleratedProxyOwnershipClass::ProxyCoherent,
                AcceleratedProxyEligibilityReason::ProxyRequiresRuntimeAcceptance, false};

    return {AcceleratedProxyOwnershipClass::ProxyCoherent,
            AcceleratedProxyEligibilityReason::EligibleControlledProxyTest, true};
}

[[nodiscard]] constexpr std::string_view AcceleratedProxyOwnershipClassName(
    AcceleratedProxyOwnershipClass value) noexcept {
    switch (value) {
    case AcceleratedProxyOwnershipClass::DirectCoherent:
        return "direct-coherent";
    case AcceleratedProxyOwnershipClass::ProxyCoherent:
        return "proxy-coherent";
    case AcceleratedProxyOwnershipClass::UnknownProxyChain:
        return "unknown-proxy-chain";
    case AcceleratedProxyOwnershipClass::InconsistentOwnership:
        return "inconsistent-ownership";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view AcceleratedProxyEligibilityReasonName(
    AcceleratedProxyEligibilityReason value) noexcept {
    switch (value) {
    case AcceleratedProxyEligibilityReason::EligibleDirect:
        return "eligible-direct";
    case AcceleratedProxyEligibilityReason::EligibleControlledProxyTest:
        return "eligible-controlled-proxy-test";
    case AcceleratedProxyEligibilityReason::MissingCanonicalDevice:
        return "missing-canonical-device";
    case AcceleratedProxyEligibilityReason::MissingImmediateContext:
        return "missing-immediate-context";
    case AcceleratedProxyEligibilityReason::ContextNotImmediate:
        return "context-not-immediate";
    case AcceleratedProxyEligibilityReason::ContextDeviceMismatch:
        return "context-device-mismatch";
    case AcceleratedProxyEligibilityReason::D3D11ModuleEvidenceUnavailable:
        return "d3d11-module-evidence-unavailable";
    case AcceleratedProxyEligibilityReason::ProxyDispatchEvidenceUnavailable:
        return "proxy-dispatch-evidence-unavailable";
    case AcceleratedProxyEligibilityReason::DispatchOwnershipInconsistent:
        return "dispatch-ownership-inconsistent";
    case AcceleratedProxyEligibilityReason::ProxyRequiresRuntimeAcceptance:
        return "proxy-requires-runtime-acceptance";
    }
    return "unknown";
}

}
