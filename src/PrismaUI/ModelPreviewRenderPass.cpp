#include "ModelPreviewInternal.h"

#include <chrono>
#include <cmath>
#include <memory>

namespace PrismaUI::ModelPreview {

float EffSpin(float s) { return s > -9999.0f ? s : g_cfg.spinDegPerSec; }

bool ComputePose(Preview& pv, std::shared_ptr<PreviewModel> pm, const std::string& key, int kind) {
    if (!pm || pm->items.empty()) return false;
    pv.model = pm; pv.modelKey = key; pv.kind = kind;
    XMStoreFloat4x4(&pv.orient, XMMatrixIdentity());
    BoundSphere bound = pm->bound;
    if (bound.r <= 0.001f && pm->fxBound.r > 0.001f) bound = pm->fxBound;
    pv.center = bound.c; pv.radius = bound.r > 0.001f ? bound.r : 1.0f;

    float ex = 0, ey = 0, ez = 0; bool haveExt = pm->box.any;
    if (haveExt) {
        ex = pm->box.mx[0] - pm->box.mn[0]; ey = pm->box.mx[1] - pm->box.mn[1]; ez = pm->box.mx[2] - pm->box.mn[2];
        XMFLOAT3 mid = { (pm->box.mn[0] + pm->box.mx[0]) * 0.5f, (pm->box.mn[1] + pm->box.mx[1]) * 0.5f,
                         (pm->box.mn[2] + pm->box.mx[2]) * 0.5f };
        float extR = 0.5f * sqrtf(ex * ex + ey * ey + ez * ez);

        pv.center = mid;
        if (extR > 0.001f && (pv.radius < 0.25f * extR || pv.radius > 4.0f * extR)) pv.radius = extR;
    }

    if (pm->hasInvMarker) {
        XMMATRIX o = XMMatrixRotationZ(-pm->invRot[2]) * XMMatrixRotationY(-pm->invRot[1]) *
                     XMMatrixRotationX(-pm->invRot[0]);
        if (kind == kKindAmmo) o = o * XMMatrixRotationX(XM_PI);
        XMStoreFloat4x4(&pv.orient, o);
    } else if (kind == kKindElongate && haveExt) {

        const float e[3] = { ex, ey, ez };
        int L = (e[0] >= e[1] && e[0] >= e[2]) ? 0 : (e[1] >= e[2] ? 1 : 2);
        int S = (e[0] <= e[1] && e[0] <= e[2]) ? 0 : (e[1] <= e[2] ? 1 : 2);
        if (S == L) S = (L + 1) % 3;
        const int M = 3 - L - S;
        XMFLOAT4X4 m{};
        m.m[L][0] = 1.0f; m.m[S][1] = 1.0f; m.m[M][2] = 1.0f; m.m[3][3] = 1.0f;
        XMMATRIX o = XMLoadFloat4x4(&m);

        if (XMVectorGetX(XMMatrixDeterminant(o)) < 0.0f) {
            m.m[S][1] = -1.0f;
            o = XMLoadFloat4x4(&m);
        }
        XMStoreFloat4x4(&pv.orient, o);
    } else if ((kind == kKindAmmo || kind == kKindGeneric) && haveExt) {

        float mn = ex < ey ? (ex < ez ? ex : ez) : (ey < ez ? ey : ez);
        float mx = ex > ey ? (ex > ez ? ex : ez) : (ey > ez ? ey : ez);
        float mid = ex + ey + ez - mn - mx;
        bool xLargest = ex >= ey && ex >= ez, yLargest = ey >= ex && ey >= ez;
        bool xSmallest = ex <= ey && ex <= ez, zSmallest = ez <= ex && ez <= ey;
        XMMATRIX o = XMMatrixIdentity(); bool changed = false;
        if (mid > 0.001f && mx > 2.0f * mid) {
            if (xLargest)      { o = XMMatrixRotationY(-XM_PIDIV2); changed = true; }
            else if (yLargest) { o = XMMatrixRotationX(XM_PIDIV2);  changed = true; }

        } else if (mn > 0.001f && mid > 1.3f * mn) {
            if (zSmallest)      { o = XMMatrixRotationX(-XM_PIDIV2); changed = true; }
            else if (xSmallest) { o = XMMatrixRotationZ(XM_PIDIV2);  changed = true; }

        }
        if (kind == kKindAmmo) o = o * XMMatrixRotationX(XM_PI);
        if (changed || kind == kKindAmmo) XMStoreFloat4x4(&pv.orient, o);
    } else if ((kind == kKindArmor || kind == kKindClothing) && haveExt) {
        float planar = ex > ey ? ex : ey;
        if (planar > 1.35f * ez) XMStoreFloat4x4(&pv.orient, XMMatrixRotationX(-XM_PIDIV2));
    }
    pv.dirty = true;
    return true;
}

bool RenderPreviewInner(ID3D11DeviceContext* ctx, Preview& pv) {
    if (!pv.model || !pv.rtv) return false;
    if (!pv.spinStarted) { pv.spinStart = std::chrono::steady_clock::now(); pv.spinStarted = true; }
    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - pv.spinStart).count();
    float spinRate = EffSpin(pv.spin);
    float angle = XMConvertToRadians(pv.yaw) + (float)(elapsed * spinRate * (XM_PI / 180.0));
    pv.lastAngleDeg = angle * (180.0f / XM_PI);

    const float clear[4] = { 0, 0, 0, 0 };
    ctx->ClearRenderTargetView(pv.rtv.Get(), clear);
    ctx->ClearDepthStencilView(g_dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    ID3D11RenderTargetView* rtv = pv.rtv.Get();
    ctx->OMSetRenderTargets(1, &rtv, g_dsv.Get());
    D3D11_VIEWPORT vp = { 0, 0, (float)g_cfg.rtSize, (float)g_cfg.rtSize, 0, 1 };
    ctx->RSSetViewports(1, &vp);
    ctx->RSSetState(g_rasterizer.Get());
    const float bf[4] = { 0, 0, 0, 0 };
    ctx->OMSetBlendState(g_blendState.Get(), bf, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(g_depthState.Get(), 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11SamplerState* samp = g_sampler.Get(); ctx->PSSetSamplers(0, 1, &samp);
    ID3D11Buffer* cb = g_cb.Get(); ctx->VSSetConstantBuffers(0, 1, &cb); ctx->PSSetConstantBuffers(0, 1, &cb);
    ctx->PSSetShader(g_ps.Get(), nullptr, 0);

    float R = pv.radius;
    float d = R / tanf(XMConvertToRadians(15.0f)) * 1.12f / pv.zoom;
    float elev = XMConvertToRadians(35.0f);
    XMVECTOR panOfs = XMVectorSet(pv.panX * R, 0, pv.panY * R, 0);
    XMVECTOR eye = XMVectorAdd(XMVectorSet(0, -d * cosf(elev), d * sinf(elev), 1), panOfs);
    XMVECTOR at = XMVectorAdd(XMVectorSet(0, 0, 0, 1), panOfs);
    XMMATRIX view = XMMatrixLookAtRH(eye, at, XMVectorSet(0, 0, 1, 0));
    XMMATRIX proj = XMMatrixPerspectiveFovRH(XMConvertToRadians(30.0f), 1.0f, 0.05f * R, d + 4.0f * R);
    XMMATRIX recenter = XMMatrixTranslation(-pv.center.x, -pv.center.y, -pv.center.z);
    XMMATRIX orient = XMLoadFloat4x4(&pv.orient);
    if (pv.flip) orient = orient * XMMatrixRotationX(XM_PI);
    if (pv.roll != 0.0f) orient = orient * XMMatrixRotationY(XMConvertToRadians(pv.roll));
    XMMATRIX spin = XMMatrixRotationZ(angle);
    if (pv.pitch != 0.0f) spin = spin * XMMatrixRotationX(XMConvertToRadians(pv.pitch));
    XMVECTOR l0 = XMVector3Normalize(XMVectorSet(0.45f, 0.80f, -0.50f, 0));
    XMVECTOR l1 = XMVector3Normalize(XMVectorSet(-0.60f, 0.40f, 0.25f, 0));

    bool texPending = false;

    auto drawOne = [&](GpuItem& item) {

        if (!g_cfg.soloShape.empty() && item.name.find(g_cfg.soloShape) == std::string::npos) return;
        ID3D11InputLayout* layout = GetOrCreateLayout(item.desc, item.vsIndex, item.floatPos);
        if (!layout) return;

        if (!item.diffusePath.empty() && !item.texSettled)
            item.texSettled = AcquireEngineTexture(item.diffusePath, item.engineTex);
        ID3D11ShaderResourceView* srv = nullptr;
        if (item.engineTex) srv = ResolveSrv(item.engineTex->rendererTexture);
        item.srvReady = (srv != nullptr);
        if (!srv) {

            if (!item.texSettled || item.engineTex) texPending = true;
            if (item.effect) return;
            srv = g_graySrv.Get();
        }
        if (item.srvFormatCachedFor != srv) {
            item.srvFormatCachedFor = srv;
            item.srvNeedsSrgbDecode = SrvNeedsShaderSrgbDecode(srv);
        }
        XMMATRIX world = XMLoadFloat4x4(&item.world) * recenter * orient * spin;
        XMMATRIX wvp = world * view * proj;
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (FAILED(ctx->Map(g_cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
        auto* dptr = (CBData*)mapped.pData;
        XMStoreFloat4x4(&dptr->wvp, XMMatrixTranspose(wvp));
        XMStoreFloat4x4(&dptr->world, XMMatrixTranspose(world));
        XMStoreFloat4(&dptr->light0, XMVectorSetW(l0, 1.1f));
        XMStoreFloat4(&dptr->light1, XMVectorSetW(l1, 0.5f));

        dptr->params = { item.alphaTestThreshold, item.blend == BlendClass::Alpha ? 1.0f : 0.0f,
                         item.effect ? 1.0f : 0.0f, pv.brightness };
        dptr->emissive = item.emissive;
        dptr->colorSpace = { item.srvNeedsSrgbDecode ? 1.0f : 0.0f,
                             g_srgbRtSupported ? 0.0f : 1.0f, 0.0f, 0.0f };
        ctx->Unmap(g_cb.Get(), 0);
        ctx->PSSetShaderResources(0, 1, &srv);
        ctx->IASetInputLayout(layout);
        ctx->VSSetShader(g_vs[item.vsIndex].Get(), nullptr, 0);
        UINT offset = 0; ID3D11Buffer* vb = item.vb.Get();
        ctx->IASetVertexBuffers(0, 1, &vb, &item.stride, &offset);
        ctx->IASetIndexBuffer(item.ib.Get(), DXGI_FORMAT_R16_UINT, 0);
        ctx->DrawIndexed(item.indexCount, 0, 0);
    };

    ctx->OMSetBlendState(g_blendState.Get(), bf, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(g_depthState.Get(), 0);
    for (auto& item : pv.model->items) if (item.blend == BlendClass::Opaque) drawOne(item);

    ctx->OMSetBlendState(g_blendAlpha.Get(), bf, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(g_depthStateNoWrite.Get(), 0);
    for (auto& item : pv.model->items) if (item.blend == BlendClass::Alpha) drawOne(item);

    ctx->OMSetBlendState(g_blendAdditive.Get(), bf, 0xFFFFFFFF);
    for (auto& item : pv.model->items) if (item.blend == BlendClass::Additive) drawOne(item);

    pv.drawn = true;
    return texPending;
}

}
