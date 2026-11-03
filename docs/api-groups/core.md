---
title: Core table
sidebar_label: Core
sidebar_position: 30
---

# Core table

One table with the original surface: view lifecycle, page messaging, health, enumeration, suppression and offscreen helpers.

Most plugins discover Core first. It contains the calls everything else builds on.

## Discover it

```cpp
CoreAPI core{};
if (!Discover<ApiFeature::Core>(CoreApiVersion, core)) {
    return;
}
```

## Methods

- [`CreateView`](../api/CreateView) - Creates an HTML view and begins loading the specified file.
- [`Invoke`](../api/Invoke) - Evaluates an arbitrary JavaScript expression in the view's context.
- [`InteropCall`](../api/InteropCall) - Calls a named JavaScript function via the JS Interop API.
- [`RegisterJSListener`](../api/RegisterJSListener) - Exposes a C++ callback to JavaScript.
- [`HasFocus`](../api/HasFocus)
- [`Focus`](../api/Focus) - Gives input focus to the view, routing keyboard and mouse events to the HTML page.
- [`Unfocus`](../api/Unfocus) - Removes focus from the view and restores game input.
- [`Show`](../api/Show) - Makes a hidden view visible at the next composite pass.
- [`Hide`](../api/Hide) - Removes a visible view from the composite.
- [`IsHidden`](../api/IsHidden)
- [`GetScrollingPixelSize`](../api/GetScrollingPixelSize) - Retained for V1 ABI/source compatibility.
- [`SetScrollingPixelSize`](../api/SetScrollingPixelSize) - Retained for V1 ABI/source compatibility.
- [`IsValid`](../api/IsValid)
- [`Destroy`](../api/Destroy) - Tears down the view completely.
- [`SetOrder`](../api/SetOrder) - Sets the view's rendering z-order among other visible views.
- [`GetOrder`](../api/GetOrder)
- [`CreateInspectorView`](../api/CreateInspectorView) - Retained for V1 ABI/source compatibility.
- [`SetInspectorVisibility`](../api/SetInspectorVisibility) - Retained for V1 ABI/source compatibility.
- [`IsInspectorVisible`](../api/IsInspectorVisible) - Retained for V1 ABI/source compatibility.
- [`SetInspectorBounds`](../api/SetInspectorBounds) - Retained for V1 ABI/source compatibility.
- [`HasAnyActiveFocus`](../api/HasAnyActiveFocus)
- [`RegisterConsoleCallback`](../api/RegisterConsoleCallback) - Receives every console.log/warn/error/debug/info call from the view's JS context.
- [`RegisterTranslations`](../api/RegisterTranslations) - Loads the current Fallout 4 translation file for pluginName, builds the window.L10N / window.t() helper script, and injects that script into the current live document through...
- [`BindUIEvent`](../api/BindUIEvent) - Same shape as RegisterJSListener: exposes a C++ callback to JavaScript as window.functionName(arg).
- [`EnumerateViews`](../api/EnumerateViews) - Enumerates every currently-registered view across all plugins, synchronously, from any thread.
- [`GetViewSRV`](../api/GetViewSRV)
- [`SetViewOffscreen`](../api/SetViewOffscreen) - Marks a view as offscreen: it keeps rendering to its texture (even while hidden with Hide) but is never drawn as a 2D screen overlay.
- [`BindViewToGeometry`](../api/BindViewToGeometry) - On-mesh rendering: binds this view's live texture onto a named piece of geometry's diffuse texture slot, so the view's pixels appear on that 3D surface and ride its UVs as it...
- [`BindViewToScreenTexture`](../api/BindViewToScreenTexture) - Like BindViewToGeometry, but finds the target geometry by its diffuse texture path (case-insensitive substring match) instead of by node name.
- [`UnbindViewFromGeometry`](../api/UnbindViewFromGeometry) - Restores the bound geometry's original texture and clears the binding created by BindViewToGeometry or BindViewToScreenTexture.
- [`SuppressHUDWidget`](../api/SuppressHUDWidget) - Hides or restores a supported vanilla HUD widget by class name, for example "HUDCompass", "HUDAmmoCounter", or "HUDPlayerHealthMeter".
- [`SuppressVanillaMenu`](../api/SuppressVanillaMenu) - Forces a full vanilla menu invisible by its MENU_NAME, e.g. "PipboyMenu".
- [`CloseVanillaMenu`](../api/CloseVanillaMenu) - Closes a vanilla menu immediately, by MENU_NAME.
- [`SuppressVanillaMenuIf`](../api/SuppressVanillaMenuIf) - Conditional version of SuppressVanillaMenu: instead of a fixed on/off flag, predicate is re-checked every time the named menu tries to open.
- [`EnableActivateChoiceFilter`](../api/EnableActivateChoiceFilter) - Enables or disables capture of the vanilla multi-activate choice row so V8 consumers can read the current choices with GetActivateChoiceLabel and trigger one with...
- [`SuppressActivateChoicePerk`](../api/SuppressActivateChoicePerk) - This method is retained as an ABI-compatible placeholder in PrismaUI_F4 2.1.0.
- [`EnumerateViewsEx`](../api/EnumerateViewsEx) - Same as EnumerateViews (V4), but the callback also reports which plugin created each view.
- [`GetActivateChoiceLabel`](../api/GetActivateChoiceLabel) - Reads the label of the vanilla multi-activate choice currently captured at a given button slot - for example "Field Dress" for a hunting-perk choice.
- [`TriggerActivateChoice`](../api/TriggerActivateChoice) - Fires the captured choice at a given button slot, replaying the same call the engine makes when the vanilla row's corresponding button is actually clicked.
- [`GetViewHealth`](../api/GetViewHealth)
- [`SetViewOffscreenSize`](../api/SetViewOffscreenSize) - Sets the resolution of a view's offscreen browser (see SetViewOffscreen, V5).
- [`SetViewOffscreenBackground`](../api/SetViewOffscreenBackground) - Sets the clear/background color associated with a view's offscreen rendering path.

## What to read with it

- [Your first panel](../quick-start)
- [View lifecycle](../view-lifecycle)

## Related

- [Modern API](../modern-api)
- [API reference](../api-reference)
- [Cookbook](/docs/cookbook/)

