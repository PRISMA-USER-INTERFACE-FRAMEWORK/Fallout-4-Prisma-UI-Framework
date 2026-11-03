# `NoteInputDevice`

**Since:** `IVPrismaUI9`

```cpp
virtual void NoteInputDevice(int device) noexcept = 0;   // 0=keyboard, 1=mouse, 2=gamepad
```

Feeds an input-device observation from a plugin's own input sink into the framework's tracked
"active device" state.

## Parameters

- `device` - `0` for keyboard, `1` for mouse, `2` for gamepad.

## When you need this

The framework self-tracks menu-context input automatically via its own input sink. A plugin that
**also** has its own gameplay-context input sink - for example, a crosshair-based quick-loot UI that
reads input while no menu is open - should call this with every device observation it sees, so the
tracked device stays correct out in the world too, not just inside menus.

## What counts as a real observation

Only real button presses claim the device. A key/button release (value `0` in the underlying event,
distinct from the `device` parameter here) and thumbstick movement are both ignored outright, not
just filtered by a deadzone - a plugged-in pad's stick drift sends a thumbstick event every frame, so
any deadzone threshold eventually gets crossed by drift alone and would falsely claim "gamepad" while
the player is actually on keyboard. A single genuine button or D-pad press still flips the device
immediately.

## See also

`IsUsingGamepad`, `GetButtonPrompt`.
