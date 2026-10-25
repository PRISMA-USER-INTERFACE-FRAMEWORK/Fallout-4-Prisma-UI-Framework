# API Reference - PrismaUI_F4 V1-V12

## The calls you will use first

Almost every plugin uses the same handful of calls. Each links to its own page with the signature, parameters and return values.

| Call | What it does |
| --- | --- |
| [`CreateView`](api/CreateView) | Load a page and return a `PrismaView` handle. |
| [`IsValid`](api/IsValid) | Check a handle before using it. |
| [`Show`](api/Show) / [`Hide`](api/Hide) | Draw or stop drawing the view. |
| [`Focus`](api/Focus) / [`Unfocus`](api/Unfocus) | Give the view input, or give input back to the game. |
| [`SetViewRole`](api/SetViewRole) | Tell other mods what kind of view this is. |
| [`BindUIEvent`](api/BindUIEvent) | Run C++ when the page calls a named function. |
| [`InteropCall`](api/InteropCall) | Call a named JavaScript function with a string payload. |
| [`Invoke`](api/Invoke) | Run one line of JavaScript in the page. |

A whole panel is usually just this cycle:

```cpp
PrismaView view = api->CreateView("MyPlugin/index.html", OnDomReady);
if (view) {
    api->SetViewRole(view, PRISMA_UI_API::ViewRole::kPanel);
    api->Hide(view);
}

// later, when the player opens it
if (api->IsValid(view)) {
    api->Show(view);
    api->Focus(view, false);
}
```

If you are new, [Your first panel](quick-start) builds that cycle step by step, and the [Cookbook](/docs/cookbook/) has complete examples for HUDs, previews, controllers and translations.


This is the current public C++ API index for PrismaUI_F4. The desktop SDK ships as one canonical `PrismaUI_F4_API.h` header containing `IVPrismaUI1` through `IVPrismaUI12`. V11 and V12 derive from the previous interfaces and append methods so the older ABI prefix does not move.

- [`PrismaUI_F4_API.h`](https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Fallout-4-Prisma-UI-Framework/blob/main/src/PrismaUI_F4_API.h) is the complete desktop V1-V12 SDK header.
- [`PrismaUI_F4VR_API.h`](https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Fallout-4-Prisma-UI-Framework/blob/main/src/PrismaUI_F4VR_API.h) is the separate VR provider/header contract.

- [`PrismaUI_F4_Modern_API.h`](https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Fallout-4-Prisma-UI-Framework/blob/main/src/PrismaUI_F4_Modern_API.h) is the preferred discovery surface for new code. It exposes independently versioned Core, Controller, GameThread, Meta, View, Interop, Localization, Render, Input, and Menu feature tables.

For new integrations, start with the [Modern API guide](modern-api). The numbered V1-V12 interface remains the compatibility ABI for existing consumers.

Include `PrismaUI_F4_API.h` and request the lowest interface you need. Always null-check `RequestPluginAPI` at runtime. An older installed provider may expose only an earlier interface.

For controller integration, see the full [Controller Actions guide](controller-actions). For ABI details and extension boundaries, see [Current API Extensions](api-extensions).

## Current desktop contract

PrismaUI_F4:

- uses Ultralight 1.4.0 in-process;
- uses the rewritten D3D11 GPU-accelerated presentation path, with CPU BitmapSurface retained as a controlled fallback;
- supports Fallout 4 `1.10.163` (OG) and `1.11.137+` (AE) with matching Address Library data;
- deliberately rejects the intermediate `1.10.980-1.10.984` runtime line;
- keeps the V1-V10 ABI prefix frozen;
- adds V11 and V12 by deriving from the previous interface and appending new methods;
- exposes V11/V12 on the flat Fallout provider only;
- keeps Fallout 4 VR on its separate provider/header contract.

## Requesting the API

Request the lowest interface containing the features your mod actually needs.

### V12, controller actions plus all earlier flat APIs

```cpp
#include "PrismaUI_F4_API.h"

static PRISMA_UI_API::IVPrismaUI12* g_api = nullptr;

g_api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI12>();
if (!g_api) {
    logger::warn("PrismaUI V12 unavailable");
    return;
}
```

### V11, verified game-thread/window-thread dispatch

```cpp
#include "PrismaUI_F4_API.h"

auto* api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI11>();
```

### V10 or earlier

Consumers that do not need V11/V12 should keep requesting the older interface they were built for. Existing V1-V10 binaries do not need to migrate just because newer interfaces exist.

## Core types

### `PrismaView`

```cpp
typedef uint64_t PrismaView;
```

Opaque view handle. `0` means no view. Use [`IsValid`](api/IsValid) before reusing a handle whose lifetime may have changed.

### `ViewRole`

```cpp
enum class ViewRole : uint32_t {
    kUnspecified = 0,
    kWidget = 1,
    kPanel = 2,
};
```

Interactive panels should declare `kPanel`; passive HUD views normally use `kWidget`.

### `InputRegion`

```cpp
struct InputRegion {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
};
```

Used by V10 selective overlay input through [`SetInputRegions`](api/SetInputRegions).

### V11 callback types

```cpp
using GameThreadTaskCallback = void (*)(void* userdata);
using GameThreadUIEventCallback = void (*)(const char* argument, void* userdata);
```

V11 uses these for verified deferred engine work and JavaScript-to-native callbacks.

## Interface versions

| Interface | Adds |
|---|---|
| `IVPrismaUI1` | Core view lifecycle, focus, visibility, ordering, inspector/scroll compatibility methods |
| `IVPrismaUI2` | `RegisterConsoleCallback` |
| `IVPrismaUI3` | `RegisterTranslations` |
| `IVPrismaUI4` | `BindUIEvent`, `EnumerateViews` |
| `IVPrismaUI5` | Offscreen/SRV/geometry handoff methods |
| `IVPrismaUI6` | Vanilla HUD/menu suppression |
| `IVPrismaUI7` | Conditional suppression and activate-choice capture compatibility methods |
| `IVPrismaUI8` | Owner-aware enumeration, activate-choice read/trigger, health, offscreen sizing |
| `IVPrismaUI9` | Controller prompts/device state, Escape ownership, offscreen background |
| `IVPrismaUI10` | View roles, panel coordination, `FocusOverlay`, `SetInputRegions` |
| `IVPrismaUI11` | Verified deferred game/window-thread dispatch and game-thread UI bindings |
| `IVPrismaUI12` | Focused-view canonical controller action routing |

## IVPrismaUI1

Core view API:

- [`CreateView`](api/CreateView)
- [`Invoke`](api/Invoke)
- [`InteropCall`](api/InteropCall)
- [`RegisterJSListener`](api/RegisterJSListener)
- [`HasFocus`](api/HasFocus)
- [`Focus`](api/Focus)
- [`Unfocus`](api/Unfocus)
- [`Show`](api/Show)
- [`Hide`](api/Hide)
- [`IsHidden`](api/IsHidden)
- [`GetScrollingPixelSize`](api/GetScrollingPixelSize)
- [`SetScrollingPixelSize`](api/SetScrollingPixelSize)
- [`IsValid`](api/IsValid)
- [`Destroy`](api/Destroy)
- [`SetOrder`](api/SetOrder)
- [`GetOrder`](api/GetOrder)
- [`CreateInspectorView`](api/CreateInspectorView)
- [`SetInspectorVisibility`](api/SetInspectorVisibility)
- [`IsInspectorVisible`](api/IsInspectorVisible)
- [`SetInspectorBounds`](api/SetInspectorBounds)
- [`HasAnyActiveFocus`](api/HasAnyActiveFocus)

The inspector and scrolling methods remain in the ABI for compatibility but are not normal Ultralight integration paths.

## IVPrismaUI2

- [`RegisterConsoleCallback`](api/RegisterConsoleCallback)

Use this to route JavaScript console output into native diagnostics.

## IVPrismaUI3

- [`RegisterTranslations`](api/RegisterTranslations)

Register translations against the live document, normally from DOM-ready.

## IVPrismaUI4

- [`BindUIEvent`](api/BindUIEvent)
- [`EnumerateViews`](api/EnumerateViews)

## IVPrismaUI5

- [`GetViewSRV`](api/GetViewSRV)
- [`SetViewOffscreen`](api/SetViewOffscreen)
- [`BindViewToGeometry`](api/BindViewToGeometry)
- [`BindViewToScreenTexture`](api/BindViewToScreenTexture)
- [`UnbindViewFromGeometry`](api/UnbindViewFromGeometry)

These are advanced rendering integrations. Normal panels do not need them.

## IVPrismaUI6

- [`SuppressHUDWidget`](api/SuppressHUDWidget)
- [`SuppressVanillaMenu`](api/SuppressVanillaMenu)
- [`CloseVanillaMenu`](api/CloseVanillaMenu)

## IVPrismaUI7

- [`SuppressVanillaMenuIf`](api/SuppressVanillaMenuIf)
- [`EnableActivateChoiceFilter`](api/EnableActivateChoiceFilter)
- [`SuppressActivateChoicePerk`](api/SuppressActivateChoicePerk)

`SuppressActivateChoicePerk` remains a compatibility slot, not a general working perk-row filter.

## IVPrismaUI8

- [`EnumerateViewsEx`](api/EnumerateViewsEx)
- [`GetActivateChoiceLabel`](api/GetActivateChoiceLabel)
- [`TriggerActivateChoice`](api/TriggerActivateChoice)
- [`GetViewHealth`](api/GetViewHealth)
- [`SetViewOffscreenSize`](api/SetViewOffscreenSize)

## IVPrismaUI9

Controller presentation and device APIs:

- [`IsUsingGamepad`](api/IsUsingGamepad)
- [`GetControllerStyle`](api/GetControllerStyle)
- [`SetControllerStyle`](api/SetControllerStyle)
- [`NoteInputDevice`](api/NoteInputDevice)
- [`GetButtonPrompt`](api/GetButtonPrompt)
- [`GetGamepadButtonName`](api/GetGamepadButtonName)
- [`SetViewOwnsEscape`](api/SetViewOwnsEscape)
- [`SetViewOffscreenBackground`](api/SetViewOffscreenBackground)

V9 is about prompt/device presentation. V12 is the API that owns focused controller action routing.

## IVPrismaUI10

Panel coordination and selective input:

- [`SetViewRole`](api/SetViewRole)
- [`GetViewRole`](api/GetViewRole)
- [`GetFocusedView`](api/GetFocusedView)
- [`IsAnyPanelVisible`](api/IsAnyPanelVisible)
- [`FocusOverlay`](api/FocusOverlay)
- [`SetInputRegions`](api/SetInputRegions)

Check the `InputRegions` capability before making selective overlay regions mandatory.

## IVPrismaUI11

V11 derives from V10 and adds the engine-safe dispatch layer used when work must run on the verified Fallout HWND owner thread.

- [`DispatchToGameThread`](api/DispatchToGameThread)
- [`IsGameThread`](api/IsGameThread)
- [`BindGameThreadUIEvent`](api/BindGameThreadUIEvent)

### `DispatchToGameThread`

```cpp
bool DispatchToGameThread(
    GameThreadTaskCallback callback,
    void* userdata) noexcept;
```

Queues work through the verified Fallout window message queue. The callback is deferred even if the caller is already on that thread so an active input/menu/detour stack can unwind first. A `false` return means the task was rejected and will not run later.

### `IsGameThread`

Returns true only when the verified owner thread is ready and the caller is executing on that exact thread. The public name is `IsGameThread`, but the concrete guarantee is the verified Fallout window thread, not independent proof about every engine subsystem.

### `BindGameThreadUIEvent`

```cpp
bool BindGameThreadUIEvent(
    PrismaView view,
    const char* functionName,
    GameThreadUIEventCallback callback,
    void* userdata) noexcept;
```

Use this for JavaScript-to-native events that mutate Fallout state, menus, Scaleform, or other engine-owned objects. `userdata` must remain valid for the required binding lifetime.

V11 is flat Fallout only. Fallout 4 VR does not advertise V11.

## IVPrismaUI12

V12 derives from V11 and adds framework-owned focused controller routing.

- [`BindControllerAction`](api/BindControllerAction)
- [`UnbindControllerAction`](api/UnbindControllerAction)
- [`ClearControllerActions`](api/ClearControllerActions)

### Public methods

```cpp
bool BindControllerAction(
    PrismaView view,
    const char* canonicalButton,
    const char* action) noexcept;

bool UnbindControllerAction(
    PrismaView view,
    const char* canonicalButton) noexcept;

void ClearControllerActions(PrismaView view) noexcept;
```

`BindControllerAction` returns false until the target view is DOM-ready. Rebinding the same button replaces the previous action deterministically. Action identifiers are framework-owned copies and use 1-64 validated ASCII characters.

### Canonical buttons

`A`, `B`, `X`, `Y`, `LB`, `RB`, `LT`, `RT`, `LS`, `RS`, `Back`, `Start`, `DUp`, `DDown`, `DLeft`, `DRight`.

### JavaScript delivery

The exact focused page receives:

```js
window.addEventListener('prisma-controller-action', ({ detail }) => {
  // detail.action
  // detail.button
  // detail.state: 'pressed', 'repeat', or 'released'
});
```

Delivery is asynchronous UI-thread work. If the action needs to mutate Fallout state, call a V11 `BindGameThreadUIEvent` listener or dispatch native work with `DispatchToGameThread`.

### Focus and consumption

A mapped event is admitted only for the exact live focused view. Once admitted, the original Fallout controller event is marked `kStop` so it does not also activate gameplay or another menu underneath the Prisma view.

`kStop` means accepted for asynchronous dispatch. It is not synchronous JavaScript acknowledgement.

Unmapped, unfocused, invalid-view, keyboard/mouse, and idle events are not consumed by this path.

### Legacy D-pad, A and B behavior

Without explicit V12 mappings, focused Prisma views keep the existing navigation route:

- D-pad -> Arrow keys
- A -> Enter
- B -> Escape

Explicitly mapping `A`, `B`, `DUp`, `DDown`, `DLeft`, or `DRight` replaces that button's synthetic-key route. One physical event is not delivered twice.

### LT and RT

Triggers use hysteresis:

- press at `0.55` or higher;
- release at `0.45` or lower;
- preserve held state through the middle band.

Trigger state resets on accepted focus acquisition so a view does not inherit stale trigger state after focus leaves and returns.

### Lifecycle

Mappings belong to a `PrismaView`. `UnbindControllerAction` removes one mapping, `ClearControllerActions` removes all mappings, and `Destroy(view)` clears mappings before backend destruction.

For the complete integration pattern, examples, prompt rendering, migration from raw controller sinks, and VR boundary, read [Controller Actions](controller-actions).

## Typical V12 call sequence

```text
kGameDataReady
  -> RequestPluginAPI<IVPrismaUI12>()
  -> null-check

CreateView
  -> SetViewRole(...)
  -> Hide(...) if the panel starts closed

OnDomReady
  -> RegisterJSListener / BindUIEvent
  -> BindGameThreadUIEvent for Fallout mutations
  -> BindControllerAction for custom controller actions

Open
  -> Show(view)
  -> Focus(...) or FocusOverlay(...)

Close
  -> Unfocus(view)
  -> Hide(view)

Destroy
  -> framework clears V12 controller mappings
```

## Papyrus bridge

PrismaUI exposes the owner-scoped `window.prisma` bridge for supported Papyrus/global/property operations. See [Papyrus Bridge](papyrus-bridge) for the access and timing contract.

## ModelPreview

The current ModelPreview bridge is `window.__prismaUI_modelPreview` API v4. See [Model Preview](model-preview) for the JavaScript contract.

## Fallout 4 VR

VR uses the separate `PrismaUI_F4VR.dll` provider and dedicated `PrismaUI_F4VR_API.h` header. The desktop header contains V11/V12 declarations, but the VR provider does not advertise those interfaces.

See [VR extension](api/vr-extension) for the VR-specific API.
