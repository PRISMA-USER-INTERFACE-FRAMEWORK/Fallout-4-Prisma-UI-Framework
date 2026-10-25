---
title: 'Modern API'
---

# Modern API

New plugins should use `PrismaUI_F4_Modern_API.h`. Instead of one large interface, it gives you small feature tables, and you ask only for the ones you need.

Existing plugins using `PrismaUI_F4_API.h` (`IVPrismaUI1` … `IVPrismaUI12`) keep working unchanged. The modern header does not deprecate them; it is a nicer way to discover the same framework.

## The idea in one example

```cpp
#include "PrismaUI_F4_Modern_API.h"

using namespace PRISMA_UI_FLAT_API;

static ViewAPI       g_viewApi{};
static InteropAPI    g_interop{};
static LocalizationAPI g_localization{};

static bool LoadPrisma()
{
    return Discover<ApiFeature::View>(ViewApiVersion, g_viewApi) &&
           Discover<ApiFeature::Interop>(InteropApiVersion, g_interop) &&
           Discover<ApiFeature::Localization>(LocalizationApiVersion, g_localization);
}
```

Each `Discover` call fills one table and returns `false` when the installed PrismaUI cannot provide it. You never call through a table you failed to discover.

## The tables

| Feature | Table | What it is for |
| --- | --- | --- |
| Core | `CoreAPI` | View lifecycle, focus, visibility, ordering, health, console, legacy translations, enumeration, rendering hooks |
| View | `ViewAPI` | Focused view lifecycle plus cursor policy |
| Interop | `InteropAPI` | `Invoke`, `InteropCall`, JS listeners, UI events, console callbacks |
| Controller | `ControllerAPI` | Device state, prompts, roles, input regions, controller actions, focus entry, native gamepad |
| GameThread | `GameThreadAPI` | Verified game-thread dispatch and UI event callbacks |
| Localization | `LocalizationAPI` | Legacy translations plus V4 JSON localization |
| Render | `RenderAPI` | Offscreen rendering, texture view and geometry/screen-texture binding |
| Input | `InputAPI` | Scrolling compatibility and selective input regions |
| Menu | `MenuAPI` | HUD/menu suppression, activate-choice capture and view enumeration |
| Meta | `MetaAPI` | Provider identity, API build and capability discovery |

Every table starts with `structSize` and `apiVersion`, so a newer framework can hand you the fields your build understands.

## Which table do I need?

- Creating and showing a panel: **View** (and **Interop** for page messaging).
- Page ↔ plugin messages: **Interop**.
- Controller buttons and prompts: **Controller**.
- Changing game state from a page event: **GameThread**.
- Translation files: **Localization**.
- HUD and menu suppression: **Menu**.
- Overlay input regions: **Controller** / **Input**.
- In-world screens: **Render**.

You can add a table later without rewriting the rest — that is the point of the split.

## Checking capabilities

When you want to know whether a feature exists before requesting another table, use the Meta table:

```cpp
MetaAPI meta{};
if (!Discover<ApiFeature::Meta>(MetaApiVersion, meta)) {
    return;
}

if (!HasCapability(meta, ApiFeature::Localization, LocalizationApiVersion)) {
    return;   // translations are not available on this build/provider
}
```

Prefer this over checking a version string. A version number tells you which release is installed; a capability tells you what it can actually do here — including on VR, where the provider advertises only what it implements.

## Controller table

The Controller table covers the V9–V12 controller surface plus native gamepad mode:

- `IsUsingGamepad`, `GetControllerStyle`, `SetControllerStyle`, `NoteInputDevice`
- `GetButtonPrompt`, `GetGamepadButtonName`
- `SetViewOwnsEscape`, `SetViewRole`, `GetViewRole`
- `GetFocusedView`, `IsAnyPanelVisible`
- `FocusOverlay`, `SetInputRegions`
- `BindControllerAction`, `UnbindControllerAction`, `ClearControllerActions`, `GetControllerActionBridgeState`
- `BindControllerFocusEntry`, `UnbindControllerFocusEntry`
- `SetNativeGamepad`

Use controller actions for menu commands. Use `SetNativeGamepad` only when the page genuinely needs live stick or trigger values through `navigator.getGamepads()`.

PrismaUI owns controller-bridge recovery. Do not build another retry loop on top.

## Localization table

```cpp
LocalizationAPI localization{};
if (!Discover<ApiFeature::Localization>(LocalizationApiVersion, localization)) return;

if (!localization.RegisterTranslationsV4(view, "MyPlugin")) {
    REX::WARN("no usable translation JSON");
}
```

V4 loads JSON through the game's resource system (loose files or BA2), follows the selected language with English fallback, supports dotted keys, and exposes `window.PrismaL10N`. The file contract is on [Translations](translations).

## Cursor policy

`ViewAPI` has `SetViewCursorPolicy` / `GetViewCursorPolicy`. Cursor policy is per view, so a page can use its own CSS cursor without fighting the compositor cursor.

## When to use the numbered API instead

- You maintain an existing plugin that already requests a numbered interface.
- You need one specific V-numbered interface and nothing else.
- You are following an older guide that was written against it.

Both headers can be used in the same plugin if you are migrating one table at a time.

## VR

The modern header can discover a VR provider, but VR advertises only the features it implements. Always discover the exact feature and null-check it — do not assume the flat provider's tables exist.

## Where to go next

- [Your first panel](quick-start) — the numbered API, complete
- [API reference](api-reference) — every call
- [Current API extensions](api-extensions) — what V11 and V12 added
- [Controller actions](controller-actions) · [Translations](translations)
