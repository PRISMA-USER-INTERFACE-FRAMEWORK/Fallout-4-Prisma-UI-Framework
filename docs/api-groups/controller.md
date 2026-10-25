---
title: Controller table
sidebar_label: Controller
sidebar_position: 31
---

# Controller table

Gamepad state, prompts, roles, overlays, controller actions and native gamepad mode.

Discover Controller when the UI is meant to be used with a gamepad, or when you need roles and overlay input.

## Discover it

```cpp
ControllerAPI controller{};
if (!Discover<ApiFeature::Controller>(ControllerApiVersion, controller)) {
    return;
}
```

## Methods

- [`IsUsingGamepad`](../api/IsUsingGamepad)
- [`GetControllerStyle`](../api/GetControllerStyle)
- [`SetControllerStyle`](../api/SetControllerStyle) - Sets the controller art style: 0 for Xbox, 1 for PlayStation.
- [`NoteInputDevice`](../api/NoteInputDevice) - Feeds an input-device reading from a plugin's own input sink into the framework's tracked "active device" state.
- [`GetButtonPrompt`](../api/GetButtonPrompt) - Resolves a vanilla ControlMap user event to a prompt token for the currently active device.
- [`GetGamepadButtonName`](../api/GetGamepadButtonName) - Resolves a raw gamepad button code to a canonical id, for a plugin that already has a button code in hand rather than a ControlMap user event name.
- [`SetViewOwnsEscape`](../api/SetViewOwnsEscape) - Opts a view into owning the Escape key while it is focused.
- [`SetViewRole`](../api/SetViewRole) - Declare what a view is. The framework uses this to answer IsAnyPanelVisible correctly - a passive HUD widget should not count as "another UI is in the way", but a full...
- [`GetViewRole`](../api/GetViewRole)
- [`GetFocusedView`](../api/GetFocusedView)
- [`IsAnyPanelVisible`](../api/IsAnyPanelVisible) - Answers the question you actually want before opening your own panel: is another Prisma UI on screen and interactive right now?
- [`FocusOverlay`](../api/FocusOverlay) - Focuses a view using the V10 overlay/input-region path.
- [`SetInputRegions`](../api/SetInputRegions) - Defines the screen-space rectangles that should be treated as interactive for a V10 overlay-focused view.
- [`BindControllerAction`](../api/BindControllerAction) - Maps one canonical controller button to an application action for a specific Prisma view.
- [`UnbindControllerAction`](../api/UnbindControllerAction) - Removes one controller-action mapping from a Prisma view.
- [`ClearControllerActions`](../api/ClearControllerActions) - Removes every V12 controller-action mapping owned by a Prisma view.
- `GetControllerActionBridgeState`
- `BindControllerFocusEntry`
- `UnbindControllerFocusEntry`
- `SetNativeGamepad`

## What to read with it

- [A controller-friendly menu](../cookbook/controller-menu)
- [A click-through HUD overlay](../cookbook/hud-overlay)

## Related

- [Modern API](../modern-api)
- [API reference](../api-reference)
- [Cookbook](/docs/cookbook/)

