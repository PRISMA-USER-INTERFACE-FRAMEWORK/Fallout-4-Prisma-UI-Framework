---
title: 'Modern API'
---
# Modern feature-table API

`PrismaUI_F4_Modern_API.h` is the preferred discovery surface for new native integrations. It does not replace the frozen `IVPrismaUI1` through `IVPrismaUI12` ABI. It provides independently versioned feature tables so a consumer can request only the framework capabilities it actually uses.

## Header

```cpp
#include "PrismaUI_F4_Modern_API.h"
```

The namespace is `PRISMA_UI_FLAT_API`.

## Feature tables

| Feature | Table | Current version | Purpose |
|---|---|---:|---|
| Core | `CoreAPI` | 1 | View lifecycle, focus, visibility, ordering, health, console, legacy translations, enumeration and core rendering hooks |
| Controller | `ControllerAPI` | 1 | Device state, prompts, roles, input regions, focused controller actions, focus entry and native Gamepad mode |
| GameThread | `GameThreadAPI` | 1 | Verified Fallout window-thread dispatch and UI event callbacks |
| Meta | `MetaAPI` | 1 | Provider identity, API build and feature discovery |
| View | `ViewAPI` | 1 | Focused view lifecycle plus cursor policy |
| Interop | `InteropAPI` | 1 | Invoke, interop calls, JS listeners, UI events and console callbacks |
| Localization | `LocalizationAPI` | 1 | Legacy translations plus V4 JSON localization |
| Render | `RenderAPI` | 1 | SRV, offscreen rendering and geometry/screen-texture binding |
| Input | `InputAPI` | 1 | Scrolling compatibility and selective input regions |
| Menu | `MenuAPI` | 1 | HUD/menu suppression, activate-choice capture and view enumeration |

All current feature tables start with `structSize` and `apiVersion`. Consumers should discover the table and use only the methods present in the returned version.

## Discover a table

```cpp
#include "PrismaUI_F4_Modern_API.h"

using namespace PRISMA_UI_FLAT_API;

ControllerAPI g_controller{};

bool LoadControllerAPI()
{
    return Discover<ApiFeature::Controller>(ControllerApiVersion, g_controller);
}
```

`Discover` clears the table, requests `PrismaUI_F4_GetAPI`, verifies the returned size, and rejects an API version below the requested minimum.

## Check provider capabilities

The Meta table can be used when a plugin wants explicit capability discovery before requesting another feature.

```cpp
using namespace PRISMA_UI_FLAT_API;

MetaAPI meta{};
if (!Discover<ApiFeature::Meta>(MetaApiVersion, meta)) {
    return;
}

if (!HasCapability(meta, ApiFeature::Localization, LocalizationApiVersion)) {
    return;
}
```

Do not infer support from a PrismaUI version string when a feature-table query is available.

## Controller API

The Controller table includes the V9-V12 controller surface plus native Gamepad mode:

- `IsUsingGamepad`
- `GetControllerStyle`
- `SetControllerStyle`
- `NoteInputDevice`
- `GetButtonPrompt`
- `GetGamepadButtonName`
- `SetViewOwnsEscape`
- `SetViewRole`
- `GetViewRole`
- `GetFocusedView`
- `IsAnyPanelVisible`
- `FocusOverlay`
- `SetInputRegions`
- `BindControllerAction`
- `UnbindControllerAction`
- `ClearControllerActions`
- `GetControllerActionBridgeState`
- `BindControllerFocusEntry`
- `UnbindControllerFocusEntry`
- `SetNativeGamepad`

Use focused controller actions for semantic UI commands. Use `SetNativeGamepad` only when a focused page needs live stick/trigger state through `navigator.getGamepads()`.

PrismaUI 2.2.0 owns controller-action bridge recovery. Consumers should not build their own retry loop around a failed bridge install.

## Localization API

The Localization table exposes both compatibility registration and V4 JSON localization:

```cpp
LocalizationAPI localization{};
if (!Discover<ApiFeature::Localization>(LocalizationApiVersion, localization)) {
    return;
}

if (!localization.RegisterTranslationsV4(view, "MyPlugin")) {
    return;
}
```

V4 loads JSON through Fallout's resource system, supports loose files and BA2 archives, detects the selected game language, falls back to English, supports dotted keys, and exposes interpolation through `window.PrismaL10N`.

See [Translations](translations.md) for the file contract.

## View cursor policy

The View table exposes `SetViewCursorPolicy` and `GetViewCursorPolicy`. Cursor policy is owned per view so a page can use its own CSS cursor without fighting the compositor cursor.

## Legacy API remains supported

`PrismaUI_F4_API.h` remains the stable vtable ABI for existing consumers. V1-V12 are unchanged by the modern discovery surface.

Use the legacy header when maintaining an existing plugin that already requests a numbered interface. Prefer the feature-table header for new integrations that want explicit capability discovery and smaller contracts.

## Fallout 4 VR

The modern header can discover a VR provider, but VR advertises only features it actually implements. Do not assume that GameThread or every flat-provider table exists in VR. Always discover the exact feature and null-check the result.

## Related

- [API Reference](api-reference.md)
- [Current API Extensions](api-extensions.md)
- [Controller Actions](controller-actions.md)
- [Translations](translations.md)
