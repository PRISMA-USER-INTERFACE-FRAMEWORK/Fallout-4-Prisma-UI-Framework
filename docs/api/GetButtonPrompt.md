# `GetButtonPrompt`

**Since:** `IVPrismaUI9`

```cpp
virtual bool GetButtonPrompt(const char* userEvent, char* outBuffer, size_t bufferSize) noexcept = 0;
```

Resolves a vanilla `ControlMap` user event to a prompt token for the currently active device.

## Parameters

- `userEvent` - A vanilla user event name, e.g. `"Activate"`, `"Ready Weapon"`.
- `outBuffer` / `bufferSize` - Destination buffer. Up to `bufferSize - 1` bytes are copied, plus a
  null terminator.

## Returns

`true` and fills `outBuffer` with either the keyboard key's display text (e.g. `"E"`), or
`"gp:<canonical>"` for a gamepad button (e.g. `"gp:A"`, `"gp:LB"`), depending on the active device.
Returns `false` (buffer untouched) if that event has no binding on the active device.

## Example

```cpp
char prompt[32];
if (api->GetButtonPrompt("Activate", prompt, sizeof(prompt))) {
    // prompt is now "E" (keyboard) or "gp:A" (gamepad) depending on the active device
    api->InteropCall(view, "setActivatePrompt", prompt);
}
```

## Rendering the prompt in JS

Every view can use the shared shell asset `prisma_controller_glyphs.js`
(`assets/views/prisma_controller_glyphs.js`, deployed alongside every plugin's views) to turn a raw
prompt token into real button art instead of drawing your own:

```javascript
// Handles both "E" and "gp:A" automatically
element.innerHTML = window.PrismaCG.renderKey(promptToken, isPlayStation);
```

`isPlayStation` is `GetControllerStyle() == 1`, typically passed down from C++ alongside the prompt
token via `InteropCall`, or read once at page init. Button art ships under
`views/icons/ControllerButtons/{XGamepad,P5Gamepad}/Default/`; canonical ids with no art file for a
given style fall back to a labelled pill.

## See also

`GetGamepadButtonName`, `NoteInputDevice`, `GetControllerStyle`.
