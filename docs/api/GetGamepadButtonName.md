# `GetGamepadButtonName`

**Since:** `IVPrismaUI9`

```cpp
virtual bool GetGamepadButtonName(uint32_t bsButtonCode, char* outBuffer, size_t bufferSize) noexcept = 0;
```

Resolves a raw gamepad button code to a canonical id, for a plugin that already has a button code
in hand rather than a `ControlMap` user event name.

## Parameters

- `bsButtonCode` - A raw `RE::BS_BUTTON_CODE` gamepad code.
- `outBuffer` / `bufferSize` - Destination buffer for the canonical id (e.g. `"A"`, `"LB"`, `"DUp"`).

## Returns

`false` if the code isn't a known gamepad button.

## Rendering

Pass the canonical id to `window.PrismaCG.chip(id, isPlayStation)` in JS to render it as button art
directly, without going through a prompt-token string first.

## See also

`GetButtonPrompt` - the higher-level call for the common case of "which button is this game action
on", rather than starting from a raw button code.
