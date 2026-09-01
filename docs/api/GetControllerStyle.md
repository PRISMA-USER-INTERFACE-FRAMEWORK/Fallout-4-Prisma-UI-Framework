# `GetControllerStyle`

**Since:** `IVPrismaUI9`

```cpp
virtual int GetControllerStyle() noexcept = 0;
```

Returns the current controller art style: `0` for Xbox, `1` for PlayStation.

Fallout 4 reads every gamepad as XInput (Xbox) natively - PlayStation is purely a display re-style
of the same canonical button, not a difference in how input is actually read.

## Persistence

The framework itself persists this setting: it is read from `PrismaUI_F4.ini`'s
`[Controller] Style = xbox | playstation` at startup (default `xbox`). A plugin does not need its
own setting for this, and the style is already consistent across every Prisma plugin in the load
order without any extra coordination.

## See also

`SetControllerStyle`, `GetButtonPrompt`, `GetGamepadButtonName`.
