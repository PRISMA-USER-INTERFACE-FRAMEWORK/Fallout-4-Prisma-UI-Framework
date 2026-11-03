---
title: View table
sidebar_label: View
sidebar_position: 34
---

# View table

The focused view surface: create, show, hide, focus, health, plus per-view cursor policy.

Discover View when the plugin only needs the lifecycle calls.

## Discover it

```cpp
ViewAPI viewApi{};
if (!Discover<ApiFeature::View>(ViewApiVersion, viewApi)) {
    return;
}
```

## Methods

- [`CreateView`](../api/CreateView) - Creates an HTML view and begins loading the specified file.
- [`Destroy`](../api/Destroy) - Tears down the view completely.
- [`Show`](../api/Show) - Makes a hidden view visible at the next composite pass.
- [`Hide`](../api/Hide) - Removes a visible view from the composite.
- [`Focus`](../api/Focus) - Gives input focus to the view, routing keyboard and mouse events to the HTML page.
- [`Unfocus`](../api/Unfocus) - Removes focus from the view and restores game input.
- [`HasFocus`](../api/HasFocus)
- [`IsValid`](../api/IsValid)
- [`IsHidden`](../api/IsHidden)
- [`GetViewHealth`](../api/GetViewHealth)
- [`SetOrder`](../api/SetOrder) - Sets the view's rendering z-order among other visible views.
- [`GetOrder`](../api/GetOrder)
- `SetViewCursorPolicy`
- `GetViewCursorPolicy`

## What to read with it

- [Your first panel](../quick-start)
- [Build a settings page](../cookbook/settings-panel)

## Related

- [Modern API](../modern-api)
- [API reference](../api-reference)
- [Cookbook](/docs/cookbook/)

