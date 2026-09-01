# `UnbindControllerAction`

**Since:** `IVPrismaUI12`

```cpp
virtual bool UnbindControllerAction(
    PrismaView view,
    const char* canonicalButton
) noexcept = 0;
```

Removes one controller-action mapping from a Prisma view.

## Parameters

- `view` - Target Prisma view handle.
- `canonicalButton` - Canonical button name used when the action was registered.

## Returns

Returns `true` when an existing mapping is removed. Returns `false` when the view/button is invalid or no mapping exists for that button.

Other mappings owned by the same view are left unchanged.

## Example

```cpp
api->UnbindControllerAction(view, "X");
```

After removal, the button falls back to the normal Prisma controller policy. For A/B/D-pad buttons, that means the legacy Arrow/Enter/Escape path can apply again while the view is focused.

## See also

- [Controller actions](../controller-actions.md)
- `BindControllerAction`
- `ClearControllerActions`
