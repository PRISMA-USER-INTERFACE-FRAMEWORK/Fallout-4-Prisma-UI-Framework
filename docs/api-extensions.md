# Current flat-provider API extensions

`PrismaUI_F4_API.h` is the complete desktop SDK header. It contains the frozen V1-V10 ABI plus additive `IVPrismaUI11` and `IVPrismaUI12` interfaces in the same file. Newer interfaces derive from the previous one and append methods so existing vtable prefixes do not move.

## Header

```cpp
#include "PrismaUI_F4_API.h"
```

That one header contains V1 through V12. Request the lowest interface version that provides the feature your mod needs.

> Always request and null-check the exact interface at runtime. An older installed provider may expose only an earlier version.

## Requesting V12

```cpp
#include "PrismaUI_F4_API.h"

static PRISMA_UI_API::IVPrismaUI12* g_api = nullptr;

g_api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI12>();
if (!g_api) {
    logger::warn("PrismaUI V12 unavailable");
    return;
}
```

## IVPrismaUI11

V11 derives from `IVPrismaUI10` and adds verified Fallout window-thread dispatch:

```cpp
virtual bool DispatchToGameThread(
    GameThreadTaskCallback callback,
    void* userdata) noexcept = 0;

virtual bool IsGameThread() noexcept = 0;

virtual bool BindGameThreadUIEvent(
    PrismaView view,
    const char* functionName,
    GameThreadUIEventCallback callback,
    void* userdata) noexcept = 0;
```

### What V11 guarantees

`DispatchToGameThread` queues work onto the verified owner thread of the Fallout HWND. Work is deferred through the window message queue even when the caller is already on that thread, allowing the current input/menu/detour stack to unwind first.

`IsGameThread` is true only after that thread has been verified and the caller is currently executing on it. The public name is retained, but the concrete guarantee is the verified Fallout window thread. It should not be interpreted as independent proof of every engine subsystem's simulation-thread identity.

`BindGameThreadUIEvent` registers a JavaScript-to-native event whose native callback is delivered on that verified thread. Use it when the callback touches Fallout-owned state, menus, Scaleform, or other engine objects.

V11 is flat Fallout 4 only. The VR provider does not advertise it.

See:

- [`DispatchToGameThread`](api/DispatchToGameThread.md)
- [`IsGameThread`](api/IsGameThread.md)
- [`BindGameThreadUIEvent`](api/BindGameThreadUIEvent.md)

## IVPrismaUI12

V12 derives from V11 and adds focused-view controller actions:

```cpp
virtual bool BindControllerAction(
    PrismaView view,
    const char* canonicalButton,
    const char* action) noexcept = 0;

virtual bool UnbindControllerAction(
    PrismaView view,
    const char* canonicalButton) noexcept = 0;

virtual void ClearControllerActions(
    PrismaView view) noexcept = 0;
```

V12 allows a consumer to map one of the framework's canonical controller buttons to an application action owned by a specific Prisma view. The focused page receives `prisma-controller-action` with `detail.action`, `detail.button`, and `detail.state`.

Bindings are accepted only after the view is DOM-ready. Mapped events are consumed only for the exact focused live view. Legacy D-pad/A/B Arrow/Enter/Escape behavior remains available for buttons that are not explicitly mapped.

Canonical buttons:

`A`, `B`, `X`, `Y`, `LB`, `RB`, `LT`, `RT`, `LS`, `RS`, `Back`, `Start`, `DUp`, `DDown`, `DLeft`, `DRight`.

V12 is flat Fallout 4 only and follows the same provider boundary as V11.

See:

- [Controller actions guide](controller-actions.md)
- [`BindControllerAction`](api/BindControllerAction.md)
- [`UnbindControllerAction`](api/UnbindControllerAction.md)
- [`ClearControllerActions`](api/ClearControllerActions.md)

## ABI compatibility

The V1-V10 interface prefix remains frozen. V11 derives from V10 and V12 derives from V11, appending only new methods. A consumer compiled against an older interface should keep requesting that older interface.

The SDK's move from separate V11/V12 include files to one `PrismaUI_F4_API.h` does not change the interface inheritance or runtime request contract. It removes duplicated consumer headers and makes one file the canonical public SDK surface.

## VR boundary

`PrismaUI_F4VR.dll` remains a separate provider with `PrismaUI_F4VR_API.h`. The presence of V11/V12 declarations in the desktop SDK is not a promise that the VR provider exposes those interfaces.
