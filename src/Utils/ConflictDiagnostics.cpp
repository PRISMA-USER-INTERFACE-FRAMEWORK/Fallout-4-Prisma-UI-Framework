#include "ConflictChecker.h"

#include <Psapi.h>
#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>
#include <wrl/client.h>

#include <string>

namespace PrismaUI::ConflictChecker {

namespace {

const wchar_t* kKnownConflictDlls[] = {
    L"ReShade.dll", L"ReShade64.dll", L"FallSouls.dll", L"FallSouls_NG.dll", L"enbseries.dll",
    L"dxvk.dll", L"d3d11_log.dll", L"RTSSHooks64.dll",
};

const wchar_t* kKnownFrameGenDlls[] = {
    L"AAAFrameGeneration.dll", L"amd_fidelityfx_dx12.dll", L"nvngx_dlssg.dll",
    L"dlssg_to_fsr3.dll", L"sl.interposer.dll", L"sl.dlss_g.dll",
};

std::string WideToUtf8(const std::wstring& value)
{
    if (value.empty()) return {};
    const int length = ::WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (length <= 0) return {};
    std::string result(static_cast<std::size_t>(length - 1), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), length, nullptr, nullptr);
    return result;
}

const char* GpuVendorName(unsigned vendorId)
{
    switch (vendorId) {
    case 0x10DE: return "NVIDIA";
    case 0x1002: return "AMD";
    case 0x8086: return "Intel";
    case 0x1414: return "Microsoft (WARP/basic)";
    default: return "unknown";
    }
}

}

void LogSystemSummary()
{
    std::string os = "Windows (version query failed)";
    if (HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll")) {
        using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
        if (auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(::GetProcAddress(ntdll, "RtlGetVersion"))) {
            OSVERSIONINFOW vi{};
            vi.dwOSVersionInfoSize = sizeof(vi);
            if (rtlGetVersion(&vi) == 0)
                os = "Windows " + std::to_string(vi.dwMajorVersion) + "." +
                     std::to_string(vi.dwMinorVersion) + " build " + std::to_string(vi.dwBuildNumber);
        }
        using WineVerFn = const char*(__cdecl*)();
        if (auto wineVer = reinterpret_cast<WineVerFn>(::GetProcAddress(ntdll, "wine_get_version"))) {
            os += " [Wine/Proton "; os += wineVer(); os += "]";
        }
    }
    logger::info("[SysInfo] os: {}", os);

    auto* rendererData = RE::BSGraphics::GetRendererData();
    auto* rawSwap = (rendererData && rendererData->renderWindow[0].swapChain)
                        ? rendererData->renderWindow[0].swapChain : nullptr;
    auto* swapChain = reinterpret_cast<IDXGISwapChain*>(rawSwap);
    if (!swapChain) {
        logger::warn("[SysInfo] no IDXGISwapChain yet -- GPU/driver/window info unavailable");
    } else {
        Microsoft::WRL::ComPtr<ID3D11Device> device;
        HRESULT hrDev = E_FAIL;
        if (rendererData && rendererData->context) {
            auto* context = reinterpret_cast<ID3D11DeviceContext*>(rendererData->context);
            context->GetDevice(device.GetAddressOf());
            hrDev = device ? S_OK : E_FAIL;
        }
        if (SUCCEEDED(hrDev) && device) {
            Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
            Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
            if (SUCCEEDED(device.As(&dxgiDevice)) && SUCCEEDED(dxgiDevice->GetAdapter(&adapter)) && adapter) {
                DXGI_ADAPTER_DESC ad{};
                adapter->GetDesc(&ad);
                std::string driver = "unknown";
                LARGE_INTEGER umd{};
                if (SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umd))) {
                    driver = std::to_string(HIWORD(umd.HighPart)) + "." + std::to_string(LOWORD(umd.HighPart)) +
                             "." + std::to_string(HIWORD(umd.LowPart)) + "." + std::to_string(LOWORD(umd.LowPart));
                }
                logger::info("[SysInfo] gpu: '{}' vendor={} (0x{:04X}) device=0x{:04X} vram={}MB driver={} "
                             "featureLevel=0x{:04X}", WideToUtf8(ad.Description), GpuVendorName(ad.VendorId),
                             ad.VendorId, ad.DeviceId, static_cast<unsigned long long>(ad.DedicatedVideoMemory) /
                             (1024ull * 1024ull), driver, static_cast<unsigned>(device->GetFeatureLevel()));
            } else {
                logger::warn("[SysInfo] gpu: no DXGI adapter -- GPU info unavailable");
            }
        } else {
            logger::warn("[SysInfo] gpu: immediate context GetDevice failed hr=0x{:08X} -- GPU/driver info unavailable",
                         static_cast<unsigned>(hrDev));
        }
        DXGI_SWAP_CHAIN_DESC scd{};
        const HRESULT hrDesc = swapChain->GetDesc(&scd);
        if (SUCCEEDED(hrDesc)) {
            BOOL fs = FALSE;
            swapChain->GetFullscreenState(&fs, nullptr);
            const char* windowMode = "windowed";
            if (!scd.Windowed || fs) windowMode = "exclusive-fullscreen";
            else if (scd.OutputWindow) {
                const LONG_PTR style = ::GetWindowLongPtrW(scd.OutputWindow, GWL_STYLE);
                windowMode = ((style & WS_CAPTION) == WS_CAPTION) ? "windowed" : "borderless";
            }
            logger::info("[SysInfo] window: modeHeuristic={} backbuffer={}x{} fmt={} buffers={} fullscreen={}",
                         windowMode, scd.BufferDesc.Width, scd.BufferDesc.Height,
                         static_cast<unsigned>(scd.BufferDesc.Format), scd.BufferCount, fs ? 1 : 0);
        } else {
            logger::warn("[SysInfo] window: swapChain->GetDesc failed hr=0x{:08X} -- window info unavailable",
                         static_cast<unsigned>(hrDesc));
        }
    }

    std::string fg, inj;
    for (const auto* dll : kKnownFrameGenDlls) {
        if (::GetModuleHandleW(dll)) { if (!fg.empty()) fg += ", "; fg += WideToUtf8(dll); }
    }
    for (const auto* dll : kKnownConflictDlls) {
        if (::GetModuleHandleW(dll)) { if (!inj.empty()) inj += ", "; inj += WideToUtf8(dll); }
    }
    logger::info("[SysInfo] frameGen: [{}]  injectors/overlays: [{}]",
                 fg.empty() ? "none detected" : fg, inj.empty() ? "none detected" : inj);
}

}
