# `IsAnyPanelVisible`

**Since:** `IVPrismaUI10`

```cpp
virtual bool IsAnyPanelVisible(PrismaView ignoreView) noexcept = 0;
```

Answers the question you actually want before opening your own panel: **is another Prisma UI on
screen and interactive right now?**

Returns `true` if any view other than `ignoreView` is currently focused, or is declared
[`kPanel`](SetViewRole.md) and not hidden. Passive HUD widgets and views that never declared a role
never make this true, so it does **not** fire just because a HUD mod is running. Pass `0` for
`ignoreView` to consider every view.

## Why not just walk `EnumerateViews`?

[`EnumerateViews`](EnumerateViews.md) reports **every** registered view, including always-on HUD
widgets (health bar, compass, ammo counter, ...). Treating "some other view exists and isn't hidden"
as "another UI is in the way" gives a permanent false positive on any setup running a HUD mod - your
own menu will refuse to open even though nothing interactive is actually up. `IsAnyPanelVisible`
exists so you don't have to guess: it only counts focused views and views explicitly declared as
interactive panels.

```cpp
// In your "open my menu" hotkey handler:
if (api->IsAnyPanelVisible(myView)) {
    return;  // another interactive Prisma panel is up; don't stack on top of it
}
api->Focus(myView);
```

## See also

- [`SetViewRole`](SetViewRole.md) - declare your view a `kPanel` so other plugins' `IsAnyPanelVisible`
  sees it, and so yours reports correctly.
- [`HasAnyActiveFocus`](HasAnyActiveFocus.md) - the V1 "is anything focused" check, if you only care
  about focus and can't require V10.
- [`GetFocusedView`](GetFocusedView.md) - which specific view holds focus.
