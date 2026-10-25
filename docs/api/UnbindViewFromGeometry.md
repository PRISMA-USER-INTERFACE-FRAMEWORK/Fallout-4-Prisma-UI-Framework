# `UnbindViewFromGeometry`

**Since:** `IVPrismaUI5`

```cpp
virtual void UnbindViewFromGeometry(PrismaView view) noexcept = 0;
```

Restores the bound geometry's original texture and clears the binding created by
`BindViewToGeometry` or `BindViewToScreenTexture`.

## Call timing

Call this while the bound geometry is still valid (for example, when the relevant menu/object is
about to close), before the underlying model is torn down.

## See also

`BindViewToGeometry`, `BindViewToScreenTexture`.
