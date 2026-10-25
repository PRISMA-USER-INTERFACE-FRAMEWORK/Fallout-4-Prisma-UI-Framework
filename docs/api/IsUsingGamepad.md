# `IsUsingGamepad`

**Since:** `IVPrismaUI9`

```cpp
virtual bool IsUsingGamepad() noexcept = 0;
```

Returns `true` if the player's last input came from a gamepad, rather than keyboard/mouse.

This is a read of the framework's own self-tracked input state - see `NoteInputDevice` for how a
plugin can contribute to that tracking outside of menu contexts.

## See also

`GetControllerStyle`, `NoteInputDevice`, `GetButtonPrompt`.
