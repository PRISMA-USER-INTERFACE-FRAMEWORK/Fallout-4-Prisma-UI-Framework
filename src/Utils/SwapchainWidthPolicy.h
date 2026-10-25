#pragma once

#include <cstdint>

namespace PrismaUI::ConflictChecker {

enum class SwapchainInterfaceProbe : std::uint8_t {
    NotSupported,
    AliasesBase,
    DistinctInterface,
    Failed,
};

struct SwapchainWidthEvidence {
    bool presentKnownProxy = false;
    bool resizeKnownProxy = false;
    bool sameProxyModule = false;
    bool unrecognizedProxy = false;
    bool presentKnownBaseOnlyFrameGen = false;
    bool resizeKnownBaseOnlyFrameGen = false;
    SwapchainInterfaceProbe swapChain4 = SwapchainInterfaceProbe::NotSupported;
    SwapchainInterfaceProbe swapChain3 = SwapchainInterfaceProbe::NotSupported;
    SwapchainInterfaceProbe swapChain2 = SwapchainInterfaceProbe::NotSupported;
    SwapchainInterfaceProbe swapChain1 = SwapchainInterfaceProbe::NotSupported;
};

[[nodiscard]] constexpr unsigned int SelectShadowSlotCount(const SwapchainWidthEvidence& evidence) noexcept
{
    const bool baseOnlyFrameGen = evidence.presentKnownBaseOnlyFrameGen &&
                                  evidence.resizeKnownBaseOnlyFrameGen && evidence.sameProxyModule;
    if (baseOnlyFrameGen) return 18;

    const auto provenWidth = [&] {
        if (evidence.swapChain4 == SwapchainInterfaceProbe::Failed ||
            evidence.swapChain3 == SwapchainInterfaceProbe::Failed ||
            evidence.swapChain2 == SwapchainInterfaceProbe::Failed ||
            evidence.swapChain1 == SwapchainInterfaceProbe::Failed)
            return 0u;
        if (evidence.swapChain4 == SwapchainInterfaceProbe::AliasesBase) return 41u;
        if (evidence.swapChain3 == SwapchainInterfaceProbe::AliasesBase) return 40u;
        if (evidence.swapChain2 == SwapchainInterfaceProbe::AliasesBase) return 36u;
        if (evidence.swapChain1 == SwapchainInterfaceProbe::AliasesBase) return 29u;
        return 18u;
    };

    if (evidence.unrecognizedProxy) return provenWidth();

    const bool recognizedProxy = evidence.presentKnownProxy || evidence.resizeKnownProxy;
    if (recognizedProxy) {
        if (!evidence.presentKnownProxy || !evidence.resizeKnownProxy || !evidence.sameProxyModule)
            return 0;
        for (const auto probe : {evidence.swapChain4, evidence.swapChain3, evidence.swapChain2,
                                 evidence.swapChain1}) {
            if (probe == SwapchainInterfaceProbe::AliasesBase || probe == SwapchainInterfaceProbe::Failed)
                return 0;
        }
        return 18;
    }

    return provenWidth();
}

}
