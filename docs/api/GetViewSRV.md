# `GetViewSRV`

**Since:** `IVPrismaUI5`

```cpp
virtual void* GetViewSRV(PrismaView view) noexcept = 0;
```

Returns the view's live D3D11 shader-resource view (`ID3D11ShaderResourceView*`), cast to `void*`
so this header does not need to pull in `d3d11.h`.

## Returns

The SRV pointer, valid as long as the view exists and has rendered at least one frame. Returns
`nullptr` if the view is invalid or has not rendered yet.

**Do not `Release()` it.** The framework owns the SRV's lifetime.

## Typical use

Feed this into your own D3D11 rendering code if you need to draw a view's contents somewhere the
framework's own on-mesh binding calls (`BindViewToGeometry`, `BindViewToScreenTexture`) don't cover.
For the common case of putting a view onto a piece of 3D geometry, prefer those calls directly.

## See also

`SetViewOffscreen`, `BindViewToGeometry`, `BindViewToScreenTexture`.
