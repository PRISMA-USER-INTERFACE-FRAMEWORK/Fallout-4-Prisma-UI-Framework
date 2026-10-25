# `SetViewOwnsEscape`

**Since:** `IVPrismaUI9`

```cpp
virtual void SetViewOwnsEscape(PrismaView view, bool owns) noexcept = 0;
```

Opts a view into owning the Escape key while it is focused.

## Parameters

- `view` - Target view handle.
- `owns` - `false` (the default) keeps the historical behavior: Escape is **not** forwarded to the
  view and reaches the game instead, toggling the vanilla pause menu. `true` delivers Escape to the
  view's JS like any other key, **and** swallows it, so the game never sees it.

## Important

Only set `owns = true` on a view that actually handles Escape in its own JavaScript. Suppressing the
key also removes the vanilla pause menu as a fallback escape hatch - if your page doesn't close
itself on Escape, the player has no way out of the focused view at all.

## Compatibility

Requires a current `PrismaUI_F4.dll` build. This method was added to `IVPrismaUI9` after that
interface's initial release rather than as a new interface version - see the compatibility note in
[api-reference.md](../api-reference.md#interface-versions).

## See also

`Focus` - the `disableFocusMenu` parameter there is a related but separate mechanism for handling
input around an existing game cursor.
