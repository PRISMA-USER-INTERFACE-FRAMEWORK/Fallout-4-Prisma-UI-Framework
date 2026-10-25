# `SetViewOffscreen`

**Since:** `IVPrismaUI5`

```cpp
virtual void SetViewOffscreen(PrismaView view, bool offscreen) noexcept = 0;
```

Marks a view as offscreen: it keeps rendering to its texture (even while hidden with `Hide`) but is
never drawn as a 2D screen overlay. Use this for on-mesh rendering, where the caller binds the
view's texture onto game geometry itself instead of letting the framework composite it as a normal
2D menu.

## Parameters

- `view` - Target view handle.
- `offscreen` - `true` to render to texture only; `false` to return to normal 2D compositing.

## Call order

Set this **before** binding the view with `BindViewToGeometry`/`BindViewToScreenTexture`, so the
view renders without also appearing as a 2D overlay. If you also intend to use
`SetViewOffscreenBackground` (V9) or `SetViewOffscreenSize` (V8), call those before
`SetViewOffscreen(view, true)` as well - both only take effect at the moment the offscreen browser
is created.

## See also

`SetViewOffscreenSize` (V8), `SetViewOffscreenBackground` (V9), `BindViewToGeometry`,
`BindViewToScreenTexture`, `UnbindViewFromGeometry`, `GetViewSRV`.
