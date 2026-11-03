# `SuppressHUDWidget`

**Since:** `IVPrismaUI6`

```cpp
virtual bool SuppressHUDWidget(const char* className, bool suppress) noexcept = 0;
```

Hides or restores a supported vanilla HUD widget by class name, for example `"HUDCompass"`, `"HUDAmmoCounter"`, or `"HUDPlayerHealthMeter"`.

## Parameters

- `className` - Supported HUD widget class name.
- `suppress` - `true` to suppress the widget, `false` to restore it.

## Returns

Returns `false` when the class name is unknown or the loaded runtime cannot resolve and validate the required HUD hook authority.

## Current runtime support

- Fallout 4 OG `1.10.163`
- Fallout 4 NG `1.10.984`
- Fallout 4 AE `1.11.137+` when matching Address Library data is available

The framework resolves named CommonLib VTABLE authorities through the loaded Address Library database and validates the hook site before applying the suppression. It fails closed rather than patching a guessed address.

## Persistence

Suppression is designed to survive HUDMenu rebuilds and save/load lifecycle changes without the consumer repeatedly reapplying the call.

## See also

- `SuppressVanillaMenu`
- `SuppressVanillaMenuIf`

**Try it:** [Vanilla UI suppression](../vanilla-ui-suppression).
