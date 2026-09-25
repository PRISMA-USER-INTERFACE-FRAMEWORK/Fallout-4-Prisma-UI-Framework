#include "ConflictChecker.h"
#include "SwapchainWidthPolicy.h"

#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>

#include <cctype>
#include <string>

namespace PrismaUI::ConflictChecker {

extern "C" const IID IID_IDXGISwapChain1;
extern "C" const IID IID_IDXGISwapChain2;
extern "C" const IID IID_IDXGISwapChain3;
extern "C" const IID IID_IDXGISwapChain4;

namespace {

bool IsDxgiAddress(void* address)
{
    const std::string owner = OwnerOf(address);
    if (owner.size() != 8) return false;
    for (std::size_t i = 0; i < owner.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(owner[i])) != "dxgi.dll"[i]) return false;
    }
    return true;
}

SwapchainInterfaceProbe ProbeInterface(IDXGISwapChain* swapChain, const IID& iid)
{
    void* queried = nullptr;
    HRESULT result = E_FAIL;
    __try {
        result = swapChain->QueryInterface(iid, &queried);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return SwapchainInterfaceProbe::Failed;
    }
    if (result == E_NOINTERFACE) return SwapchainInterfaceProbe::NotSupported;
    if (FAILED(result) || !queried) return SwapchainInterfaceProbe::Failed;
    const bool same = queried == swapChain;
    __try {
        reinterpret_cast<IUnknown*>(queried)->Release();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return SwapchainInterfaceProbe::Failed;
    }
    return same ? SwapchainInterfaceProbe::AliasesBase : SwapchainInterfaceProbe::DistinctInterface;
}

}

unsigned int DetermineShadowSlotCount(IDXGISwapChain* swapChain)
{
    if (!swapChain) return 0;
    void** dispatch = *reinterpret_cast<void***>(swapChain);
    if (!dispatch) return 0;
    const bool presentFrameGen = IsKnownFrameGenAddress(dispatch[8]);
    const bool resizeFrameGen = IsKnownFrameGenAddress(dispatch[13]);
    const bool presentEnb = IsKnownEnbProxyAddress(dispatch[8]);
    const bool resizeEnb = IsKnownEnbProxyAddress(dispatch[13]);
    const bool unrecognizedProxy =
        (!presentFrameGen && !presentEnb && !IsDxgiAddress(dispatch[8])) ||
        (!resizeFrameGen && !resizeEnb && !IsDxgiAddress(dispatch[13]));
    SwapchainWidthEvidence evidence;
    evidence.presentKnownProxy = presentFrameGen || presentEnb;
    evidence.resizeKnownProxy = resizeFrameGen || resizeEnb;
    evidence.sameProxyModule = SameModuleIdentity(dispatch[8], dispatch[13]);
    evidence.unrecognizedProxy = unrecognizedProxy;
    evidence.presentKnownBaseOnlyFrameGen = IsKnownBaseOnlyFrameGenAddress(dispatch[8]);
    evidence.resizeKnownBaseOnlyFrameGen = IsKnownBaseOnlyFrameGenAddress(dispatch[13]);
    evidence.swapChain4 = ProbeInterface(swapChain, IID_IDXGISwapChain4);
    evidence.swapChain3 = ProbeInterface(swapChain, IID_IDXGISwapChain3);
    evidence.swapChain2 = ProbeInterface(swapChain, IID_IDXGISwapChain2);
    evidence.swapChain1 = ProbeInterface(swapChain, IID_IDXGISwapChain1);
    return SelectShadowSlotCount(evidence);
}

}
