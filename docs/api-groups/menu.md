---
title: Menu table
sidebar_label: Menu
sidebar_position: 39
---

# Menu table

HUD and menu suppression, activate-choice capture and view enumeration.

Discover Menu when replacing or hiding parts of the vanilla UI.

## Discover it

```cpp
MenuAPI menu{};
if (!Discover<ApiFeature::Menu>(MenuApiVersion, menu)) {
    return;
}
```

## Methods

- [`SuppressHUDWidget`](../api/SuppressHUDWidget) - Hides or restores a supported vanilla HUD widget by class name, for example "HUDCompass", "HUDAmmoCounter", or "HUDPlayerHealthMeter".
- [`SuppressVanillaMenu`](../api/SuppressVanillaMenu) - Forces a full vanilla menu invisible by its MENU_NAME, e.g. "PipboyMenu".
- [`CloseVanillaMenu`](../api/CloseVanillaMenu) - Closes a vanilla menu immediately, by MENU_NAME.
- [`SuppressVanillaMenuIf`](../api/SuppressVanillaMenuIf) - Conditional version of SuppressVanillaMenu: instead of a fixed on/off flag, predicate is re-checked every time the named menu tries to open.
- [`EnableActivateChoiceFilter`](../api/EnableActivateChoiceFilter) - Enables or disables capture of the vanilla multi-activate choice row so V8 consumers can read the current choices with GetActivateChoiceLabel and trigger one with...
- [`SuppressActivateChoicePerk`](../api/SuppressActivateChoicePerk) - This method is retained as an ABI-compatible placeholder in PrismaUI_F4 2.1.0.
- [`EnumerateViews`](../api/EnumerateViews) - Enumerates every currently-registered view across all plugins, synchronously, from any thread.
- [`EnumerateViewsEx`](../api/EnumerateViewsEx) - Same as EnumerateViews (V4), but the callback also reports which plugin created each view.
- [`GetActivateChoiceLabel`](../api/GetActivateChoiceLabel) - Reads the label of the vanilla multi-activate choice currently captured at a given button slot - for example "Field Dress" for a hunting-perk choice.
- [`TriggerActivateChoice`](../api/TriggerActivateChoice) - Fires the captured choice at a given button slot, replaying the same call the engine makes when the vanilla row's corresponding button is actually clicked.

## What to read with it

- [Vanilla UI suppression](../vanilla-ui-suppression)

## Related

- [Modern API](../modern-api)
- [API reference](../api-reference)
- [Cookbook](/docs/cookbook/)

