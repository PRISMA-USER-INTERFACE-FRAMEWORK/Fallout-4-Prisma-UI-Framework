# `HasAnyActiveFocus`

**Since:** `IVPrismaUI1`

```cpp
virtual bool HasAnyActiveFocus() noexcept = 0;
```

Returns `true` if any PrismaUI view (from any plugin) currently has input focus.

Use this in hotkey handlers to avoid triggering gameplay actions while some menu is open - for
example, skip a "quick loot" hotkey's action if `HasAnyActiveFocus()` is true, since the player is
probably typing into a text field rather than trying to loot something.

## See also

`HasFocus` - the single-view version of this check.
