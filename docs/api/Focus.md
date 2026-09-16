# `Focus`

**Since:** `IVPrismaUI1`

```cpp
virtual bool Focus(
    PrismaView view,
    bool pauseGame = false,
    bool disableFocusMenu = false
) noexcept = 0;
```

Gives input focus to the view, routing keyboard and mouse events to the HTML page.

## Parameters

- `view` - Target view handle.
- `pauseGame` - If `true`, sets game time scale to 0. Restored on `Unfocus`. Use for menus that need
  the player's exclusive attention.
- `disableFocusMenu` - If `false` (the default), PrismaUI shows a Scaleform overlay ("FocusMenu")
  that manages the cursor and intercepts Escape to unfocus the view. If `true`, that overlay is
  suppressed: keyboard events reach the HTML `keydown` handler directly, and whatever cursor the
  game already has active (e.g. the PauseMenu's) stays active.

## When to use `disableFocusMenu = true`

When your view opens on top of an existing game menu that already shows its own cursor (e.g. the
PauseMenu). With `false`, the FocusMenu overlay would intercept Escape before your JS ever saw it,
and could conflict with the existing cursor. With `true`, your page's own `keydown` handler is
responsible for closing the view on Escape.

## When to use `disableFocusMenu = false` (default)

Standard toggle menus opened directly from gameplay, with no other game menu present.

## Returns

`true` on success.

## See also

`Unfocus`, `HasFocus`, `SetViewOwnsEscape` (V9) - an alternative way to handle Escape yourself
inside the page instead of relying on the FocusMenu overlay.
