---
title: Input table
sidebar_label: Input
sidebar_position: 38
---

# Input table

Scrolling compatibility and selective input regions for overlays.

Discover Input when an overlay should only take clicks in specific rectangles.

## Discover it

```cpp
InputAPI input{};
if (!Discover<ApiFeature::Input>(InputApiVersion, input)) {
    return;
}
```

## Methods

- [`GetScrollingPixelSize`](../api/GetScrollingPixelSize) - Retained for V1 ABI/source compatibility.
- [`SetScrollingPixelSize`](../api/SetScrollingPixelSize) - Retained for V1 ABI/source compatibility.
- [`SetInputRegions`](../api/SetInputRegions) - Defines the screen-space rectangles that should be treated as interactive for a V10 overlay-focused view.

## What to read with it

- [A click-through HUD overlay](../cookbook/hud-overlay)
- [Panel management](../panel-management)

## Related

- [Modern API](../modern-api)
- [API reference](../api-reference)
- [Cookbook](/docs/cookbook/)

