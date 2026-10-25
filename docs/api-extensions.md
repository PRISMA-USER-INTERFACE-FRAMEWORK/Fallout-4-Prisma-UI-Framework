# Current API extensions

`PrismaUI_F4_API.h` is the numbered interface: one header containing `IVPrismaUI1` through `IVPrismaUI12`. New code can instead use `PrismaUI_F4_Modern_API.h` with feature tables — see [Modern API](modern-api).

This page covers what the two newest numbered interfaces add and when you actually need them.

## Request the version you need

```cpp
#include "PrismaUI_F4_API.h"

static PRISMA_UI_API::IVPrismaUI12* g_api = nullptr;

g_api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI12>();
if (!g_api) {
    REX::WARN("PrismaUI V12 unavailable");
    return;
}
```

Always null-check. An older PrismaUI install may stop at an earlier version, and the request returns null instead of handing you an interface that does not have the methods.

## V11 — game-thread work

V11 adds the "do real engine work safely" calls:

```cpp
virtual bool DispatchToGameThread(GameThreadTaskCallback callback, void* userdata) noexcept = 0;
virtual bool IsGameThread() noexcept = 0;
virtual bool BindGameThreadUIEvent(
    PrismaView view,
    const char* functionName,
    GameThreadUIEventCallback callback,
    void* userdata) noexcept = 0;
```

What the guarantees mean in practice:

- `DispatchToGameThread` queues work onto the verified game window thread. It is **always deferred** — even if you are already on that thread, the callback waits for the current menu/input stack to unwind. A `false` return means the work was refused and will never run.
- `IsGameThread` is true only when that thread is verified and you are currently on it. It is not a general test for every engine subsystem.
- `BindGameThreadUIEvent` registers a page event whose callback runs on that verified thread — the explicit choice for mutating Fallout state from a page action.

V11 is flat-provider only. VR does not expose it.

Recipe: [Run game-thread work from a button](cookbook/run-game-thread-work).
API pages: [`DispatchToGameThread`](api/DispatchToGameThread) · [`IsGameThread`](api/IsGameThread) · [`BindGameThreadUIEvent`](api/BindGameThreadUIEvent).

## V12 — controller actions

V12 adds focused-view controller actions:

```cpp
virtual bool BindControllerAction(PrismaView view, const char* canonicalButton, const char* action) noexcept = 0;
virtual bool UnbindControllerAction(PrismaView view, const char* canonicalButton) noexcept = 0;
virtual void ClearControllerActions(PrismaView view) noexcept = 0;
```

- Bind after DOM-ready; a view that is not ready returns `false`.
- Actions are delivered only to the focused, live view, as `prisma-controller-action` with `action`, `button` and `state`.
- Unmapped D-pad/A/B keep their normal Arrow/Enter/Escape navigation.
- Canonical buttons: `A`, `B`, `X`, `Y`, `LB`, `RB`, `LT`, `RT`, `LS`, `RS`, `Back`, `Start`, `DUp`, `DDown`, `DLeft`, `DRight`.

V12 follows the same flat-provider boundary as V11.

Recipe: [A controller-friendly menu](cookbook/controller-menu).
Guide: [Controller actions](controller-actions).

## ABI compatibility

- V1–V10 are frozen. V11 derives from V10 and V12 from V11; they only append methods.
- A plugin built against an older interface should keep requesting that older interface.
- V11/V12 used to be separate include files; they are now part of `PrismaUI_F4_API.h`. The inheritance and runtime request contract did not change — there is simply one public SDK header.

## VR

`PrismaUI_F4VR.dll` is a separate provider with its own `PrismaUI_F4VR_API.h`. Seeing V11/V12 in the desktop header does not mean VR exposes them.

## Where to go next

- [Modern API](modern-api) — feature tables, the preferred surface for new code
- [API reference](api-reference) — all methods
- [Controller actions](controller-actions) · [Run game-thread work from a button](cookbook/run-game-thread-work)
