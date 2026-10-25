# `BindViewToScreenTexture`

**Since:** `IVPrismaUI5`

```cpp
virtual bool BindViewToScreenTexture(
    PrismaView view,
    void* rootObject,
    const char* textureSubstring
) noexcept = 0;
```

Like `BindViewToGeometry`, but finds the target geometry by its **diffuse texture path**
(case-insensitive substring match) instead of by node name.

## Parameters

- `view` - Target view handle. Call `SetViewOffscreen(view, true)` on it first.
- `rootObject` - An `RE::NiAVObject*` passed as `void*`.
- `textureSubstring` - A case-insensitive substring to match against geometry diffuse texture paths
  under `rootObject`.

## Why this exists

Node names vary between different models of the same in-game object, but a shared texture name
often doesn't. Every Pip-Boy replacer, for example, maps its screen to a texture named
`PipBoyScreen_d` (the engine assigns the Pip-Boy render target by that texture name), so
`BindViewToScreenTexture(view, root, "pipboyscreen")` binds correctly regardless of which specific
Pip-Boy model is equipped.

## Returns

`true` on success, same caching behavior as `BindViewToGeometry`.

## See also

`BindViewToGeometry`, `UnbindViewFromGeometry`.
