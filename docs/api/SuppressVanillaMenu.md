# `SuppressVanillaMenu`

**Since:** `IVPrismaUI6`

```cpp
virtual bool SuppressVanillaMenu(const char* menuName, bool suppress) noexcept = 0;
```

Forces a full vanilla menu invisible by its `MENU_NAME`, e.g. `"PipboyMenu"`.

## Parameters

- `menuName` - The vanilla menu's registered name.
- `suppress` - `true` to suppress, `false` to stop suppressing.

## Persistence

Unlike `SuppressHUDWidget`, a fresh `IMenu` instance is created every time a menu opens, so this
suppression is automatically reapplied every time the named menu reopens, for as long as
suppression is active for that name - you do not need to call this again on every open.

## See also

`SuppressVanillaMenuIf` (V7) - a conditional version that re-checks a predicate on every open
instead of using a fixed on/off flag. `CloseVanillaMenu`.
