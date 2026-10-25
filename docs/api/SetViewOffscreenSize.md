# `SetViewOffscreenSize`

**Since:** `IVPrismaUI8`

```cpp
virtual void SetViewOffscreenSize(PrismaView view, int width, int height) noexcept = 0;
```

Sets the resolution of a view's offscreen browser (see `SetViewOffscreen`, V5).

## Parameters

- `view` - Target view handle.
- `width` / `height` - Pixel resolution. No-op if either is `<= 0`.

## Match the target quad's aspect ratio, not the game window

The rendered texture is stretched over the target geometry's `0..1` UV range, so mismatched aspect
ratios visibly squash or stretch the page. For example, a 16:9 page bound onto a screen whose UVs
are closer to a 1.21:1 aspect comes out squashed roughly 32% horizontally - size the offscreen
browser to something like `1308x1080` for that target instead of a plain 16:9 resolution.

## Call timing

Safe to call **before** `SetViewOffscreen(view, true)` - the size is remembered and applied at the
moment the offscreen browser is actually created, so the page lays out correctly from its first
frame instead of reflowing partway through loading. Calling it on an already-live offscreen browser
resizes it in place instead.

## See also

`SetViewOffscreen`, `SetViewOffscreenBackground` (V9), `BindViewToGeometry`,
`BindViewToScreenTexture`.
