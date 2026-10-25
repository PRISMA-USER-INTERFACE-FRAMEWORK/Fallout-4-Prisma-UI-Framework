# `BindViewToGeometry`

**Since:** `IVPrismaUI5`

```cpp
virtual bool BindViewToGeometry(
    PrismaView view,
    void* rootObject,
    const char* geometryName
) noexcept = 0;
```

On-mesh rendering: binds this view's live texture onto a named piece of geometry's diffuse texture
slot, so the view's pixels appear on that 3D surface and ride its UVs as it animates.

## Parameters

- `view` - Target view handle. Call `SetViewOffscreen(view, true)` on it first, so it renders
  without also appearing as a 2D overlay.
- `rootObject` - An `RE::NiAVObject*` (e.g. `player->Get3D(true)`) passed as `void*` so this header
  does not need to pull in CommonLib types.
- `geometryName` - The `BSGeometry` node name to find under that root, e.g. `"Screen:0"` for a
  Pip-Boy-style screen.

## Returns

`true` on success. The geometry's original texture is cached internally so `UnbindViewFromGeometry`
can restore it later.

## Call timing

Call this **after** the target geometry exists in the scenegraph (for example, after the relevant
menu/object has actually opened), and re-bind each time it does - the underlying model can be
rebuilt between sessions, which invalidates a previous binding.

## See also

`BindViewToScreenTexture` - binds by diffuse-texture name instead of node name, useful when you want
to target a class of objects (e.g. "any Pip-Boy model") rather than one specific node name.
`UnbindViewFromGeometry`, `SetViewOffscreen`, `SetViewOffscreenBackground` (V9).
