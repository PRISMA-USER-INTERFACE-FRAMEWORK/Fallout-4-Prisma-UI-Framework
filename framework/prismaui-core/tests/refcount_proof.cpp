// Ownership regression proof for PrismaPrivate #75 / #76 (Frame Generation backbuffer).
//
// This is an ASSERTING test, not a print harness: it returns 0 only when every
// ownership invariant holds, and nonzero (with a [FAIL] line) otherwise, so CI and
// `xmake test` can gate on it.
//
// It models, with faithful COM refcount semantics, what the compositor draw does when
// it calls swapChain->GetBuffer(0, ...). Two GetBuffer contracts are modelled exactly:
//
//   1. A spec-compliant IDXGISwapChain AddRefs the buffer it hands back (COM contract):
//        *ppSurface = buffer; buffer->AddRef();
//   2. The AMD Frame Generation proxy (doodlum/fo4test -> AAAFrameGeneration.dll,
//      class DX12SwapChain) hands back a raw pointer WITHOUT AddRef:
//        *ppSurface = swapChainBufferProxy->resource.get();   // no AddRef -- the #75 bug
//
// Our compositor receives it into a Microsoft::WRL::ComPtr, which Releases on scope exit.
// Against contract (2) that Release is unmatched: it consumes a reference the compositor
// never received, and it is the proxy's own persistent hold that gets cancelled.
//
// Three compositor code paths are compared:
//   PerFrame_v2        - current 2.0 main: GetBuffer every frame (over-releases the proxy
//                        within a few frames -> immediate use-after-free).
//   PerResize_76       - PR #76 as merged: GetBuffer once per RTV (re)build. Stops the
//                        per-frame bleed, but STILL does one unmatched Release per build,
//                        so the proxy's ref is one short and each resize leaks it further
//                        until the backbuffer is freed while the proxy still points at it.
//   PerResize_BorrowFix- the fix: for a known FG proxy, treat GetBuffer's result as a
//                        borrowed pointer and Detach the ComPtr (never Release it); for a
//                        normal swapchain, Release normally. Balances BOTH.
//
// Build:  g++ -std=c++17 -O0 tests/refcount_proof.cpp -o refcount_proof && ./refcount_proof

#include <cstdio>
#include <string>

// --------------------------------------------------------------------------------------
static int g_failures = 0;
static void check(bool cond, const std::string& what) {
    if (!cond) { printf("     [FAIL] %s\n", what.c_str()); ++g_failures; }
}

// A minimal ref-counted COM object, like ID3D11Texture2D. The real runtime frees the
// object when refs reaches 0; any touch after that is a use-after-free. We flag a free
// that happens while the proxy still legitimately holds a reference.
struct Resource {
    long refs = 0;
    bool proxyReleased = false;      // set true ONLY at legitimate teardown
    bool freedWhileProxyHeld = false;
    long AddRef() { return ++refs; }
    long Release() {
        --refs;
        if (refs <= 0 && !proxyReleased) freedWhileProxyHeld = true;  // UAF: freed too early
        return refs;
    }
};

// The two GetBuffer contracts.
enum class Chain { Compliant, ProxyFG };
static Resource* GetBuffer(Chain c, Resource* buf) {
    if (c == Chain::Compliant) buf->AddRef();   // COM contract
    return buf;                                 // ProxyFG: raw pointer, NO AddRef
}

// Mirrors ConflictChecker::IsFrameGenProxy(swapChain): the proxy's GetBuffer slot is the
// borrowed, non-AddRef variant. This is the exact signal the real compositor keys off.
static bool Borrowed(Chain c) { return c == Chain::ProxyFG; }

// A render-target view holds one reference to the resource it views (real D3D11 behaviour).
struct RTV {
    Resource* res = nullptr;
    void create(Resource* r) { reset(); res = r; res->AddRef(); }
    void reset() { if (res) { res->Release(); res = nullptr; } }
    bool valid() const { return res != nullptr; }
};

enum class Mode { PerFrame_v2, PerResize_76, PerResize_BorrowFix };

// One compositor instance: the cached RTV plus the identity PerFrame_v2 compares against.
struct Compositor {
    RTV rtv;
    Resource* rtvIdentity = nullptr;

    // Models the compositor draw's backbuffer acquisition for a single present.
    void draw(Mode mode, Chain chain, Resource* buf) {
        const bool borrowed = Borrowed(chain);
        if (mode == Mode::PerFrame_v2) {
            // GetBuffer EVERY frame into a ComPtr that releases at scope exit; CreateRTV
            // only when the returned identity changed.
            Resource* bb = GetBuffer(chain, buf);
            if (bb != rtvIdentity || !rtv.valid()) { rtv.create(bb); rtvIdentity = bb; }
            buf->Release();                          // ComPtr dtor -- unconditional
        } else {
            // Acquire only when the cache is empty (post-invalidate / once per resize).
            if (!rtv.valid()) {
                Resource* bb = GetBuffer(chain, buf);
                rtv.create(bb);                      // CreateRenderTargetView AddRefs
                if (mode == Mode::PerResize_76) {
                    buf->Release();                  // #76 as merged: ComPtr always releases
                } else {                             // PerResize_BorrowFix
                    if (!borrowed) buf->Release();   // detach the borrow for FG proxy; release otherwise
                }
            }
        }
    }
    void invalidate() { rtv.reset(); }               // compositor InvalidateRenderTarget (on resize)
};

struct Result { long steadyRefs; long afterTeardownRefs; bool freedEarly; };

// Setup: the proxy owns one persistent ref (winrt com_ptr), the engine holds its own
// cached backbuffer RTV (BSGraphics). Then: initial render phase, N resize cycles, then
// teardown where engine + compositor drop their RTVs and only the proxy's hold should remain.
static Result runScenario(Mode mode, Chain chain, int framesPerPhase, int resizeCycles) {
    Resource tex;
    tex.AddRef();                               // proxy's own persistent hold
    RTV engineRtv; engineRtv.create(&tex);      // engine's cached backbuffer RTV

    Compositor comp;
    for (int f = 0; f < framesPerPhase; ++f) comp.draw(mode, chain, &tex);
    const long steady = tex.refs;

    for (int r = 0; r < resizeCycles; ++r) {
        comp.invalidate();                      // ResizeBuffers -> InvalidateRenderTarget
        for (int f = 0; f < framesPerPhase; ++f) comp.draw(mode, chain, &tex);
    }

    comp.invalidate();                          // compositor drops its RTV
    engineRtv.reset();                          // engine drops its RTV
    const long afterTeardown = tex.refs;        // should be exactly the proxy's 1 hold
    const bool freedEarly = tex.freedWhileProxyHeld;

    tex.proxyReleased = true; tex.Release();    // legitimate final release by the proxy
    return { steady, afterTeardown, freedEarly };
}

static const char* chainName(Chain c) { return c == Chain::Compliant ? "compliant" : "proxy(FG)"; }
static const char* modeName(Mode m) {
    switch (m) {
        case Mode::PerFrame_v2:         return "PER-FRAME (2.0 main)";
        case Mode::PerResize_76:        return "PER-RESIZE (#76)";
        default:                        return "PER-RESIZE+BORROW-FIX";
    }
}

static Result report(Mode mode, Chain chain) {
    Result r = runScenario(mode, chain, /*framesPerPhase*/ 5, /*resizeCycles*/ 3);
    printf("  %-24s | %-10s | steady refs=%ld  post-teardown=%ld  %s\n",
           modeName(mode), chainName(chain), r.steadyRefs, r.afterTeardownRefs,
           r.freedEarly ? "<-- FREED while proxy still held (UAF)" : "alive throughout");
    return r;
}

int main() {
    printf("PrismaPrivate #75/#76 backbuffer ownership proof\n");
    printf("(3 owners expected in steady state: proxy hold + engine RTV + compositor RTV)\n\n");

    // The correct steady-state refcount, independent of swapchain type, is 3.
    const long kOwners = 3;

    printf("== current 2.0 main (GetBuffer every frame) ==\n");
    Result pfCompliant = report(Mode::PerFrame_v2, Chain::Compliant);
    Result pfProxy     = report(Mode::PerFrame_v2, Chain::ProxyFG);
    printf("\n== PR #76 as merged (GetBuffer once per resize) ==\n");
    Result p76Compliant = report(Mode::PerResize_76, Chain::Compliant);
    Result p76Proxy      = report(Mode::PerResize_76, Chain::ProxyFG);
    printf("\n== proposed fix (borrowed pointer for FG proxy) ==\n");
    Result fxCompliant = report(Mode::PerResize_BorrowFix, Chain::Compliant);
    Result fxProxy     = report(Mode::PerResize_BorrowFix, Chain::ProxyFG);

    printf("\n-- assertions --\n");

    // (A) The fix must be correct for BOTH swapchain types: never a premature free, the
    //     ownership count balanced at exactly `kOwners`, and only the proxy's hold left
    //     after teardown (no leak, no early free).
    for (Result r : { fxCompliant, fxProxy }) {
        check(!r.freedEarly,               "fix: backbuffer must never be freed while the proxy holds it");
        check(r.steadyRefs == kOwners,     "fix: steady-state refs must equal the number of real owners (3)");
        check(r.afterTeardownRefs == 1,    "fix: after teardown only the proxy's single hold may remain");
    }

    // (B) The fix must be load-bearing: the paths it replaces must actually break on the
    //     proxy. If these ever stop reproducing, the model drifted and the proof is void.
    check(pfProxy.freedEarly,              "sanity: current 2.0 main must UAF the proxy (per-frame over-release)");
    check(p76Proxy.freedEarly,             "sanity: #76 alone must still UAF the proxy across resizes");
    check(p76Proxy.steadyRefs == kOwners - 1,
                                           "sanity: #76's proxy steady refs are one short of the owner count");

    // (C) The fix must not regress the ordinary swapchain the old paths already handled.
    check(!pfCompliant.freedEarly,         "regression: current 2.0 main compliant path stays alive");
    check(!p76Compliant.freedEarly,        "regression: #76 compliant path must stay alive");
    check(p76Compliant.steadyRefs == kOwners, "regression: #76 compliant refs already balanced at 3");

    if (g_failures == 0) {
        printf("\nAll ownership invariants hold. PASS.\n");
        return 0;
    }
    printf("\n%d assertion(s) FAILED.\n", g_failures);
    return 1;
}
