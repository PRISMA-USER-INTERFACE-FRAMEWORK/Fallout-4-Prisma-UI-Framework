---
title: Render table
sidebar_label: Render
sidebar_position: 37
---

# Render table

Offscreen rendering, texture views and binding a view to geometry or a screen material.

Discover Render only for advanced integrations such as in-world screens.

## Discover it

```cpp
RenderAPI render{};
if (!Discover<ApiFeature::Render>(RenderApiVersion, render)) {
    return;
}
```

## Methods

- [`GetViewSRV`](../api/GetViewSRV)
- [`SetViewOffscreen`](../api/SetViewOffscreen) - Marks a view as offscreen: it keeps rendering to its texture (even while hidden with Hide) but is never drawn as a 2D screen overlay.
- [`SetViewOffscreenSize`](../api/SetViewOffscreenSize) - Sets the resolution of a view's offscreen browser (see SetViewOffscreen, V5).
- [`SetViewOffscreenBackground`](../api/SetViewOffscreenBackground) - Sets the clear/background color associated with a view's offscreen rendering path.
- [`BindViewToGeometry`](../api/BindViewToGeometry) - On-mesh rendering: binds this view's live texture onto a named piece of geometry's diffuse texture slot, so the view's pixels appear on that 3D surface and ride its UVs as it...
- [`BindViewToScreenTexture`](../api/BindViewToScreenTexture) - Like BindViewToGeometry, but finds the target geometry by its diffuse texture path (case-insensitive substring match) instead of by node name.
- [`UnbindViewFromGeometry`](../api/UnbindViewFromGeometry) - Restores the bound geometry's original texture and clears the binding created by BindViewToGeometry or BindViewToScreenTexture.

## What to read with it

- [Put a view on an in-world screen](../cookbook/in-world-screen)
- [Model preview](../model-preview)

## Related

- [Modern API](../modern-api)
- [API reference](../api-reference)
- [Cookbook](/docs/cookbook/)

