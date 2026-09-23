# `SetViewOffscreenBackground`

**Since:** `IVPrismaUI9`

```cpp
virtual void SetViewOffscreenBackground(PrismaView view, uint32_t argb) noexcept = 0;
```

Sets the clear/background color associated with a view's offscreen rendering path.

## Parameters

- `view` - target view handle.
- `argb` - ARGB clear color. `0` requests transparent clear color.

## PrismaUI_F4 2.1.0 behavior

The Ultralight backend stores the requested clear color and applies it to the view's live `BitmapSurface` when available.

Unlike the retired legacy-runtime implementation, **2.1.0 does not require this call to happen before `SetViewOffscreen(view, true)` solely because of browser-creation timing**. The current backend can apply the color to an existing live surface.

```cpp
g_api->SetViewOffscreenBackground(view, 0x00000000);
g_api->SetViewOffscreen(view, true);
```

It is still reasonable to set the desired background before enabling offscreen mode so the intended state is established before presentation begins, but that is an application-order preference, not the old legacy-runtime creation-time limitation.

## Transparency

A transparent clear color is useful when an offscreen page is meant to composite over another surface instead of presenting an opaque black backing.

The page's own CSS still matters:

```css
html, body {
  background: transparent;
}
```

## Compatibility

This method was appended to V9 before V10 was introduced. Consumers using it should require a current PrismaUI_F4 release rather than assuming every historical V9 binary exposed the late-added slot.

## See also

[`SetViewOffscreen`](SetViewOffscreen.md), [`SetViewOffscreenSize`](SetViewOffscreenSize.md), `BindViewToGeometry`, `BindViewToScreenTexture`.
