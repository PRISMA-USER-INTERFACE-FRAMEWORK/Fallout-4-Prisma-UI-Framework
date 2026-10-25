# `SetViewRole` / `GetViewRole`

**Since:** `IVPrismaUI10`

```cpp
enum class ViewRole : uint32_t {
    kUnspecified = 0,  // default -- never counted as an interactive panel
    kWidget = 1,       // passive always-on overlay (HUD element); never blocks anything
    kPanel = 2,        // interactive panel that occupies the screen and takes input
};

virtual void SetViewRole(PrismaView view, ViewRole role) noexcept = 0;
virtual ViewRole GetViewRole(PrismaView view) noexcept = 0;
```

Declare what a view is. The framework uses this to answer
[`IsAnyPanelVisible`](IsAnyPanelVisible.md) correctly - a passive HUD widget should not count as
"another UI is in the way", but a full interactive menu should.

- `kUnspecified` (the default for every view) never counts as an interactive panel.
- `kWidget` - a passive, always-on overlay like a health bar or compass. Never blocks anything.
- `kPanel` - an interactive panel that takes over the screen and receives input. Counts as "in the
  way" while it is valid and not hidden.

Set your view's role once, e.g. in your `OnDomReady` callback:

```cpp
api->SetViewRole(myMenuView, PRISMA_UI_API::ViewRole::kPanel);
api->SetViewRole(myHudWidget, PRISMA_UI_API::ViewRole::kWidget);
```

The role is cleared automatically when the view is [`Destroy`](Destroy.md)ed.

## See also

- [`IsAnyPanelVisible`](IsAnyPanelVisible.md) - the question roles exist to answer.
