#include "WebCompositor.h"

#include <DirectXTK/WICTextureLoader.h>
#include <d3dcompiler.h>
#include <d3d11.h>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <memory>
#include <utility>
#include <vector>

#include "../Engine/EngineBackbuffer.h"
#include "Utils/ConflictChecker.h"
#include "Utils/D3D11StateGuard.h"
#include "Utils/DllLoader.h"
#include "Utils/ModulePath.h"

namespace {
    constexpr uint64_t kOverlaySourceCacheKeepTicks = 64;
    constexpr size_t kOverlaySourceCacheMaxEntries = 128;
    constexpr uint32_t kDrawErrorLogEvery = 600;

    bool g_spriteBatchFaulted = false;

    int MemoryFaultFilter(unsigned long code) {
        return (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR) ? EXCEPTION_EXECUTE_HANDLER
                                                                                       : EXCEPTION_CONTINUE_SEARCH;
    }

    bool SehInvoke(void (*fn)(void*), void* ctx) {
        __try {
            fn(ctx);
            return true;
        } __except (MemoryFaultFilter(GetExceptionCode())) {
            return false;
        }
    }

    template <class F>
    bool RunSehGuarded(F& f) {
        return SehInvoke([](void* p) { (*static_cast<F*>(p))(); }, &f);
    }

    bool ShouldLogDrawError(uint32_t& counter) { return counter++ % kDrawErrorLogEvery == 0; }
}
namespace PrismaUI {
    void WebCompositor::DrawModelOverlays(const std::vector<ModelPreview::Overlay>& overlays) {
        const uint64_t overlayTick = ++m_overlayTick;
        const auto pruneCache = [this, overlayTick]() {
            std::erase_if(m_overlaySourceSizes, [overlayTick](const auto& entry) {
                return overlayTick - entry.second.lastSeenTick > kOverlaySourceCacheKeepTicks;
            });
            while (m_overlaySourceSizes.size() > kOverlaySourceCacheMaxEntries) {
                const auto oldest = std::min_element(
                    m_overlaySourceSizes.begin(), m_overlaySourceSizes.end(),
                    [](const auto& a, const auto& b) { return a.second.lastSeenTick < b.second.lastSeenTick; });
                if (oldest == m_overlaySourceSizes.end()) break;
                m_overlaySourceSizes.erase(oldest);
            }
        };
        if (overlays.empty()) {
            pruneCache();
            return;
        }
        if (g_spriteBatchFaulted) {
            pruneCache();
            return;
        }
        if (!m_spriteBatch || !m_commonStates) {
            try {
                m_commonStates = std::make_unique<DirectX::CommonStates>(m_device.Get());
                m_spriteBatch = std::make_unique<DirectX::SpriteBatch>(m_context);
            } catch (const std::exception& e) {
                logger::critical("[WebCompositor] DirectXTK sprite init failed: {}", e.what());
                m_commonStates.reset();
                m_spriteBatch.reset();
                pruneCache();
                return;
            }
        }
        static uint32_t overlayErrors = 0;
        try {
            auto drawAll = [&]() {
            m_spriteBatch->Begin(DirectX::SpriteSortMode_Deferred, m_commonStates->AlphaBlend());
            for (const auto& o : overlays) {
                if (!o.srv) continue;
                RECT dest{o.destLeft, o.destTop, o.destRight, o.destBottom};
                if (!o.hasClip) {
                    m_spriteBatch->Draw(o.srv, dest);
                    continue;
                }
                const RECT c{o.clipLeft, o.clipTop, o.clipRight, o.clipBottom};
                RECT vis{(std::max)(dest.left, c.left), (std::max)(dest.top, c.top), (std::min)(dest.right, c.right),
                         (std::min)(dest.bottom, c.bottom)};
                if (vis.right <= vis.left || vis.bottom <= vis.top) continue;
                UINT sourceWidth = o.sourceWidth;
                UINT sourceHeight = o.sourceHeight;
                if (sourceWidth == 0 || sourceHeight == 0) {
                    auto cached = m_overlaySourceSizes.find(o.srv);
                    if (cached != m_overlaySourceSizes.end()) {
                        sourceWidth = cached->second.width;
                        sourceHeight = cached->second.height;
                        cached->second.lastSeenTick = overlayTick;
                    } else {
                        D3D11_TEXTURE2D_DESC td{};
                        Microsoft::WRL::ComPtr<ID3D11Resource> res;
                        o.srv->GetResource(&res);
                        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
                        if (res && SUCCEEDED(res.As(&tex))) tex->GetDesc(&td);
                        sourceWidth = td.Width;
                        sourceHeight = td.Height;
                        if (sourceWidth != 0 && sourceHeight != 0) {
                            OverlaySourceSize entry;
                            entry.srv = o.srv;
                            entry.width = sourceWidth;
                            entry.height = sourceHeight;
                            entry.lastSeenTick = overlayTick;
                            m_overlaySourceSizes.emplace(o.srv, std::move(entry));
                        }
                    }
                }
                if (sourceWidth == 0 || sourceHeight == 0) {
                    m_spriteBatch->Draw(o.srv, vis);
                    continue;
                }
                const float dw = float(dest.right - dest.left), dh = float(dest.bottom - dest.top);
                const float sx = float(sourceWidth) / dw, sy = float(sourceHeight) / dh;
                RECT src{long((vis.left - dest.left) * sx), long((vis.top - dest.top) * sy),
                         long((vis.right - dest.left) * sx), long((vis.bottom - dest.top) * sy)};
                m_spriteBatch->Draw(o.srv, vis, &src);
            }
            m_spriteBatch->End();
            };
            if (!RunSehGuarded(drawAll)) {
                g_spriteBatchFaulted = true;
                logger::critical("[WebCompositor] model overlay draw hit an access violation; overlays and cursor disabled for this session");
                (void)m_spriteBatch.release();
            }
        } catch (const std::exception& e) {
            if (ShouldLogDrawError(overlayErrors))
                logger::error("[WebCompositor] model overlay draw failed (x{}): {}", overlayErrors, e.what());
            m_spriteBatch.reset();
        }
        pruneCache();
    }
    void WebCompositor::DrawCursor(int x, int y, bool draw, float scaleX, float scaleY) {
        if (!draw || g_spriteBatchFaulted) return;
        if (!m_spriteBatch || !m_commonStates) {
            try {
                m_commonStates = std::make_unique<DirectX::CommonStates>(m_device.Get());
                m_spriteBatch = std::make_unique<DirectX::SpriteBatch>(m_context);
            } catch (const std::exception& e) {
                logger::critical("[WebCompositor] DirectXTK cursor init failed: {}", e.what());
                m_commonStates.reset();
                m_spriteBatch.reset();
                return;
            }
        }
        if (!m_cursorTex && !m_cursorInitTried) {
            m_cursorInitTried = true;
            const auto cursorPath = Utils::GetBasePath() / "misc" / "cursor.png";
            const HRESULT hr =
                DirectX::CreateWICTextureFromFile(m_device.Get(), cursorPath.wstring().c_str(), nullptr, &m_cursorTex);
            if (SUCCEEDED(hr)) {
                logger::info("[WebCompositor] cursor.png loaded from '{}'", cursorPath.string());
            } else {
                logger::error("[WebCompositor] cursor.png load failed from '{}' hr=0x{:08X}", cursorPath.string(),
                              static_cast<unsigned int>(hr));
                m_cursorTex.Reset();
            }
        }
        if (!m_cursorTex) return;
        static uint32_t cursorErrors = 0;
        try {
            auto drawCursor = [&]() {
            m_spriteBatch->Begin(DirectX::SpriteSortMode_Deferred, m_commonStates->AlphaBlend());
            const float spriteScaleX = (scaleX > 0.0F) ? scaleX : 1.0F;
            const float spriteScaleY = (scaleY > 0.0F) ? scaleY : 1.0F;
            m_spriteBatch->Draw(m_cursorTex.Get(),
                                DirectX::XMFLOAT2(static_cast<float>(x) * spriteScaleX,
                                                  static_cast<float>(y) * spriteScaleY),
                                nullptr, DirectX::Colors::White, 0.0F, DirectX::XMFLOAT2(0.0F, 0.0F),
                                DirectX::XMFLOAT2(spriteScaleX, spriteScaleY));
            m_spriteBatch->End();
            };
            if (!RunSehGuarded(drawCursor)) {
                g_spriteBatchFaulted = true;
                logger::critical("[WebCompositor] cursor draw hit an access violation; overlays and cursor disabled for this session");
                (void)m_spriteBatch.release();
            }
        } catch (const std::exception& e) {
            if (ShouldLogDrawError(cursorErrors))
                logger::error("[WebCompositor] cursor draw failed (x{}): {}", cursorErrors, e.what());
            m_spriteBatch.reset();
        }
    }
}
