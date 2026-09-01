# `SuppressVanillaMenuIf`

**Since:** `IVPrismaUI7`

```cpp
virtual void SuppressVanillaMenuIf(const char* menuName, MenuSuppressPredicate predicate) noexcept = 0;
```

Conditional version of `SuppressVanillaMenu`: instead of a fixed on/off flag, `predicate` is
re-checked every time the named menu tries to open.

## Parameters

- `menuName` - The vanilla menu's registered name.
- `predicate` - A `bool (*)()` checked on every open attempt. Return `true` to suppress that open,
  `false` to let it through. Runs synchronously on the game thread - keep it cheap and do not block.
  Pass `nullptr` to unregister.

## When the predicate returns true

The menu is hidden immediately via both `SetVisible(false)` (so it never renders a single frame) and
a queued `kHide` message (so it actually leaves the menu stack, rather than sitting there invisible
while still eating input).

## Use case

Suppression that depends on live game state - for example, only suppressing a menu while the player
holds a modifier key, rather than suppressing it unconditionally for the whole session.

## See also

`SuppressVanillaMenu` - the fixed-flag version.
