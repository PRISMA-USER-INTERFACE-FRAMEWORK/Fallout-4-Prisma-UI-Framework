#include "ModelPreviewInternal.h"

#include <d3dcompiler.h>

#include <cstring>
#include <string>
#include <vector>

namespace PrismaUI::ModelPreview {

static const char* SHADER_SRC = R"hlsl(
cbuffer CB : register(b0) { float4x4 gWVP; float4x4 gWorld; float4 gLight0; float4 gLight1; float4 gParams; float4 gEmissive; float4 gColorSpace; };
Texture2D gDiffuse : register(t0); SamplerState gSamp : register(s0);
struct VSIn { float4 pos : POSITION;
#ifdef HAS_UV
    float2 uv : TEXCOORD0;
#endif
#ifdef HAS_NORMAL
    float4 nrm : NORMAL;
#endif
};
struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; float3 nrm : NORMAL0; };
VSOut VSMain(VSIn i) {
    VSOut o; o.pos = mul(float4(i.pos.xyz, 1.0), gWVP);
#ifdef HAS_UV
    o.uv = i.uv;
#else
    o.uv = float2(0.5, 0.5);
#endif
#ifdef HAS_NORMAL
    o.nrm = normalize(mul(i.nrm.xyz * 2.0 - 1.0, (float3x3)gWorld));
#else
    o.nrm = float3(0.0, -1.0, 0.3);
#endif
    return o;
}
float3 SrgbToLinear(float3 c) {
    c = saturate(c);
    float3 lo = c / 12.92;
    float3 hi = pow(abs(c + 0.055) / 1.055, 2.4);
    return lerp(lo, hi, step(0.04045, c));
}
float3 LinearToSrgb(float3 c) {
    c = saturate(c);
    float3 lo = c * 12.92;
    float3 hi = 1.055 * pow(abs(c), 1.0 / 2.4) - 0.055;
    return lerp(lo, hi, step(0.0031308, c));
}
// gColorSpace.x = decode diffuse to linear here; gColorSpace.y = encode to sRGB here. Both are 0 when
// the sampler / RTV already do it in hardware. Lighting below assumes LINEAR input.
float4 PSMain(VSOut i) : SV_Target {
    float4 tex = gDiffuse.Sample(gSamp, i.uv);
    if (gColorSpace.x > 0.5) tex.rgb = SrgbToLinear(tex.rgb);

    if (gParams.x >= 0.0) clip(tex.a - gParams.x);
    if (gParams.z > 0.5) {
        float glow = dot(tex.rgb, float3(0.299,0.587,0.114));
        float3 fx = tex.rgb;
        if (gColorSpace.y > 0.5) fx = LinearToSrgb(fx);
        return float4(fx, glow * tex.a);
    }
    float3 n = normalize(i.nrm);
    float bright = gParams.w;
    float lit = (0.48 + saturate(dot(n, -gLight0.xyz)) * gLight0.w + saturate(dot(n, -gLight1.xyz)) * gLight1.w) * bright;
    float3 viewDir = float3(0.0, -0.82, 0.57);
    float rim = pow(1.0 - saturate(dot(n, viewDir)), 2.5) * 0.45 * bright;

    float outA = (gParams.y > 0.5) ? tex.a : 1.0;

    float3 emit = gEmissive.rgb * gEmissive.a * 2.0;
    float3 col = tex.rgb * lit + rim * float3(0.9,0.95,1.0) + emit;
    if (gColorSpace.y > 0.5) col = LinearToSrgb(col);
    return float4(col, outA);
}
)hlsl";

static bool g_d3dFailed = false;

static bool CompileShaders() {
    UINT flags = D3DCOMPILE_OPTIMIZATION_LEVEL2;
    for (int i = 0; i < 4; ++i) {
        D3D_SHADER_MACRO macros[3] = {}; int n = 0;
        if (i & 1) macros[n++] = { "HAS_UV", "1" };
        if (i & 2) macros[n++] = { "HAS_NORMAL", "1" };
        ComPtr<ID3DBlob> err;
        if (FAILED(D3DCompile(SHADER_SRC, strlen(SHADER_SRC), "ModelPreview", macros, nullptr, "VSMain",
                              "vs_5_0", flags, 0, &g_vsBlob[i], &err))) {
            logger::error("[ModelPreview] VS compile {} failed: {}", i,
                          err ? (const char*)err->GetBufferPointer() : "?");
            return false;
        }
        if (FAILED(g_dev->CreateVertexShader(g_vsBlob[i]->GetBufferPointer(), g_vsBlob[i]->GetBufferSize(),
                                             nullptr, &g_vs[i]))) return false;
    }
    ComPtr<ID3DBlob> psBlob, err;
    if (FAILED(D3DCompile(SHADER_SRC, strlen(SHADER_SRC), "ModelPreview", nullptr, nullptr, "PSMain",
                          "ps_5_0", flags, 0, &psBlob, &err))) {
        logger::error("[ModelPreview] PS compile failed: {}", err ? (const char*)err->GetBufferPointer() : "?");
        return false;
    }
    return SUCCEEDED(g_dev->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_ps));
}

bool EnsureRenderResources(ID3D11Device* dev) {
    if (g_d3dReady) return true;
    if (g_d3dFailed) return false;
    g_dev = dev;
    auto fail = [&](const char* what) { logger::error("[ModelPreview] D3D init failed at {}", what);
        g_d3dFailed = true; return false; };

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = td.Height = (UINT)g_cfg.rtSize; td.MipLevels = 1; td.ArraySize = 1;
    td.SampleDesc = { 1, 0 }; td.Usage = D3D11_USAGE_DEFAULT;
    td.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(dev->CreateTexture2D(&td, nullptr, &g_depthTex))) return fail("depth");
    if (FAILED(dev->CreateDepthStencilView(g_depthTex.Get(), nullptr, &g_dsv))) return fail("dsv");

    D3D11_BUFFER_DESC bd = {}; bd.ByteWidth = sizeof(CBData); bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER; bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(dev->CreateBuffer(&bd, nullptr, &g_cb))) return fail("cb");

    D3D11_SAMPLER_DESC sd = {}; sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP; sd.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(dev->CreateSamplerState(&sd, &g_sampler))) return fail("sampler");

    D3D11_RASTERIZER_DESC rd = {}; rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    if (FAILED(dev->CreateRasterizerState(&rd, &g_rasterizer))) return fail("raster");

    D3D11_DEPTH_STENCIL_DESC dsd = {}; dsd.DepthEnable = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; dsd.DepthFunc = D3D11_COMPARISON_LESS;
    if (FAILED(dev->CreateDepthStencilState(&dsd, &g_depthState))) return fail("depth state");

    D3D11_DEPTH_STENCIL_DESC dsn = dsd; dsn.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    if (FAILED(dev->CreateDepthStencilState(&dsn, &g_depthStateNoWrite))) return fail("depth state (no-write)");

    D3D11_BLEND_DESC bld = {}; bld.RenderTarget[0].BlendEnable = FALSE;
    bld.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(dev->CreateBlendState(&bld, &g_blendState))) return fail("blend");

    D3D11_BLEND_DESC abd = {}; auto& art = abd.RenderTarget[0];
    art.BlendEnable = TRUE;
    art.SrcBlend = D3D11_BLEND_SRC_ALPHA;  art.DestBlend = D3D11_BLEND_INV_SRC_ALPHA; art.BlendOp = D3D11_BLEND_OP_ADD;
    art.SrcBlendAlpha = D3D11_BLEND_ONE;   art.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA; art.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    art.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(dev->CreateBlendState(&abd, &g_blendAlpha))) return fail("blend (alpha)");

    art.DestBlend = D3D11_BLEND_ONE; art.DestBlendAlpha = D3D11_BLEND_ONE;
    if (FAILED(dev->CreateBlendState(&abd, &g_blendAdditive))) return fail("blend (additive)");

    uint32_t gray = 0xFF808080;
    D3D11_TEXTURE2D_DESC gd = {}; gd.Width = gd.Height = 1; gd.MipLevels = 1; gd.ArraySize = 1;
    gd.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; gd.SampleDesc = { 1, 0 }; gd.Usage = D3D11_USAGE_IMMUTABLE;
    gd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA gi = { &gray, 4, 0 };
    ComPtr<ID3D11Texture2D> grayTex;
    if (FAILED(dev->CreateTexture2D(&gd, &gi, &grayTex))) return fail("gray tex");
    if (FAILED(dev->CreateShaderResourceView(grayTex.Get(), nullptr, &g_graySrv))) return fail("gray srv");

    {
        UINT sup = 0;
        g_srgbRtSupported =
            SUCCEEDED(dev->CheckFormatSupport(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, &sup)) &&
            (sup & D3D11_FORMAT_SUPPORT_RENDER_TARGET) != 0;
        logger::info("[ModelPreview][srgb] preview RT encode = {} (B8G8R8A8_UNORM_SRGB render-target "
                     "support={}). Lighting runs in LINEAR; the RT is handed to the compositor as plain "
                     "_UNORM so its sRGB-encoded bytes pass through unconverted.",
                     g_srgbRtSupported ? "HARDWARE (_SRGB RTV)" : "SHADER (manual LinearToSrgb)",
                     g_srgbRtSupported);
    }

    if (!CompileShaders()) { g_d3dFailed = true; return false; }

    {
        static bool s_envLogged = false;
        if (!s_envLogged) {
            s_envLogged = true;
            const char* rt = REX::FModule::IsRuntimeOG() ? "OG" : "NOT-OG (NG/AE)";
            std::string adapter = "unknown";
            Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDev;
            Microsoft::WRL::ComPtr<IDXGIAdapter> adap;
            if (SUCCEEDED(dev->QueryInterface(IID_PPV_ARGS(&dxgiDev))) && dxgiDev &&
                SUCCEEDED(dxgiDev->GetAdapter(&adap)) && adap) {
                DXGI_ADAPTER_DESC ad{};
                if (SUCCEEDED(adap->GetDesc(&ad))) {
                    int n = WideCharToMultiByte(CP_UTF8, 0, ad.Description, -1, nullptr, 0, nullptr, nullptr);
                    if (n > 0) { adapter.assign((size_t)n - 1, '\0');
                                 WideCharToMultiByte(CP_UTF8, 0, ad.Description, -1, adapter.data(), n,
                                                     nullptr, nullptr); }
                    adapter += " (" + std::to_string(ad.DedicatedVideoMemory / (1024ull * 1024ull)) + " MB VRAM)";
                }
            }
            logger::warn("[ModelPreview] ENV: runtime={} featureLevel=0x{:04X} gpu={}",
                         rt, static_cast<unsigned>(dev->GetFeatureLevel()), adapter);
        }
    }
    const UINT devFlags = dev->GetCreationFlags();
    const bool singleThreaded = (devFlags & D3D11_CREATE_DEVICE_SINGLETHREADED) != 0;
    SetDeviceSingleThreaded(singleThreaded);
    if (singleThreaded) {
        logger::error("[ModelPreview] device has D3D11_CREATE_DEVICE_SINGLETHREADED (flags=0x{:08X}) -- "
                      "worker-thread GPU resource creation is UNSAFE on this device; GPU buffers are "
                      "created on the render thread, parsing stays on workers.", devFlags);
    } else {
        logger::info("[ModelPreview] device creation flags=0x{:08X}, SINGLETHREADED bit CLEAR as "
                     "reported through the loaded d3d11 wrapper (ENB proxies it) -- not independently "
                     "verified", devFlags);
    }

    logger::warn("[ModelPreview] EFFECTIVE model-load path = {} (bWorkerThreads={}, devSingleThreaded={})",
                 g_cfg.workerThreads ? "WORKER POOL" : "RENDER THREAD (inline)",
                 g_cfg.workerThreads ? 1 : 0, singleThreaded ? 1 : 0);

    g_d3dReady = true;
    logger::info("[ModelPreview] D3D resources ready ({}x{})", g_cfg.rtSize, g_cfg.rtSize);
    return true;
}

ID3D11InputLayout* GetOrCreateLayout(uint64_t desc, int vsIndex, bool floatPos) {
    auto it = g_layouts.find(desc);
    if (it != g_layouts.end()) return it->second.Get();
    std::vector<D3D11_INPUT_ELEMENT_DESC> e;
    e.push_back({ "POSITION", 0, floatPos ? DXGI_FORMAT_R32G32B32A32_FLOAT : DXGI_FORMAT_R16G16B16A16_FLOAT,
                  0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 });
    if (vsIndex & 1) e.push_back({ "TEXCOORD", 0, DXGI_FORMAT_R16G16_FLOAT, 0, DescAttrOffset(desc, 1),
                                   D3D11_INPUT_PER_VERTEX_DATA, 0 });
    if (vsIndex & 2) e.push_back({ "NORMAL", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, DescAttrOffset(desc, 3),
                                   D3D11_INPUT_PER_VERTEX_DATA, 0 });
    ComPtr<ID3D11InputLayout> layout;
    if (FAILED(g_dev->CreateInputLayout(e.data(), (UINT)e.size(), g_vsBlob[vsIndex]->GetBufferPointer(),
                                        g_vsBlob[vsIndex]->GetBufferSize(), &layout))) {

        logger::error("[ModelPreview] CreateInputLayout FAILED for vertex desc 0x{:X} (vs{}) -- every mesh "
                      "with this layout will fall back to the flat icon for the rest of the session",
                      desc, vsIndex);
        g_layouts[desc] = nullptr; return nullptr;
    }
    g_layouts[desc] = layout; return layout.Get();
}

static constexpr size_t kRtPoolCap = 64;

void RebuildOverlayCache() {
    if (g_overlayCacheFrame == g_previewFrame) return;
    g_overlayCache.clear();
    g_overlayCacheFrame = g_previewFrame;
    if (!g_d3dReady) return;

    for (auto& [key, pv] : g_previews) {
        if (pv.viewId == 0 || !pv.model || !pv.srv || !pv.drawn ||
            ImageSourcePolicy::ExcludesHoleOverlay(pv.image))
            continue;
        Overlay o;
        o.srv = pv.srv.Get();
        o.contentGeneration = pv.lastRenderFrame + 1;
        o.continuityKey = pv.continuityKey;
        o.sourceWidth = static_cast<uint32_t>(g_cfg.rtSize);
        o.sourceHeight = static_cast<uint32_t>(g_cfg.rtSize);
        o.destLeft = (long)pv.rect.x; o.destTop = (long)pv.rect.y;
        o.destRight = (long)(pv.rect.x + pv.rect.w); o.destBottom = (long)(pv.rect.y + pv.rect.h);
        if (pv.clip.w > 0 && pv.clip.h > 0) {
            o.hasClip = true;
            o.clipLeft = (long)pv.clip.x; o.clipTop = (long)pv.clip.y;
            o.clipRight = (long)(pv.clip.x + pv.clip.w); o.clipBottom = (long)(pv.clip.y + pv.clip.h);
        }
        g_overlayCache[pv.viewId].push_back(o);
    }
}

bool EnsurePreviewTarget(Preview& pv) {

    if (pv.tex && pv.rtv && pv.srv) return true;
    pv.tex.Reset(); pv.rtv.Reset(); pv.srv.Reset();

    if (!g_rtFreeList.empty()) {

        RtTarget t = std::move(g_rtFreeList.back());
        g_rtFreeList.pop_back();
        pv.tex = std::move(t.tex); pv.rtv = std::move(t.rtv); pv.srv = std::move(t.srv);
        ++g_rtLive; pv.dirty = true; pv.drawn = false; return true;
    }

    D3D11_TEXTURE2D_DESC td = {}; td.Width = td.Height = (UINT)g_cfg.rtSize; td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = g_srgbRtSupported ? DXGI_FORMAT_B8G8R8A8_TYPELESS : DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc = { 1, 0 };
    td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    ComPtr<ID3D11Texture2D> tex;
    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11ShaderResourceView> srv;
    if (FAILED(g_dev->CreateTexture2D(&td, nullptr, &tex))) {
        logger::error("[ModelPreview] CreateTexture2D FAILED for a {}x{} preview RT ({} live) -- likely VRAM "
                      "exhaustion; preview will be blank", g_cfg.rtSize, g_cfg.rtSize, g_rtLive);
        return false;
    }

    D3D11_RENDER_TARGET_VIEW_DESC rtvd = {};
    rtvd.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    rtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
    srvd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MipLevels = 1;

    if (FAILED(g_dev->CreateRenderTargetView(tex.Get(), g_srgbRtSupported ? &rtvd : nullptr, &rtv))) {
        logger::error("[ModelPreview] CreateRenderTargetView FAILED for a preview RT -- preview will be blank");
        return false;
    }
    if (FAILED(g_dev->CreateShaderResourceView(tex.Get(), g_srgbRtSupported ? &srvd : nullptr, &srv))) {
        logger::error("[ModelPreview] CreateShaderResourceView FAILED for a preview RT -- preview will be blank");
        return false;
    }

    pv.tex = std::move(tex); pv.rtv = std::move(rtv); pv.srv = std::move(srv);
    if (++g_rtLive > kRtPoolCap)
        logger::warn("[ModelPreview] {} concurrent preview RTs (past soft cap {})", g_rtLive, kRtPoolCap);
    pv.dirty = true; pv.drawn = false; return true;
}

void ReleaseTarget(Preview& pv) {

    const bool complete = pv.tex && pv.rtv && pv.srv;
    if (complete) {
        if (g_rtFreeList.size() < kRtPoolCap) {
            RtTarget t; t.tex = std::move(pv.tex); t.rtv = std::move(pv.rtv); t.srv = std::move(pv.srv);
            g_rtFreeList.push_back(std::move(t));
        }
        if (g_rtLive) --g_rtLive;
    }
    pv.tex.Reset(); pv.rtv.Reset(); pv.srv.Reset();
}

}
