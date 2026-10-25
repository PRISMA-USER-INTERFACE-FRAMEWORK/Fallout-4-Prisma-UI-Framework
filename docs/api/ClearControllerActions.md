# `ClearControllerActions`

**Since:** `IVPrismaUI12`

```cpp
virtual void ClearControllerActions(
    PrismaView view
) noexcept = 0;
```

Removes every V12 controller-action mapping owned by a Prisma view.

## Parameters

- `view` - Target Prisma view handle.

## Behavior

Clearing is view-local. It does not affect mappings registered by other Prisma views.

`Destroy(view)` also clears controller mappings as part of view teardown, so consumers do not need to call this immediately before destroying a view. Use `ClearControllerActions` when a live view changes modes or temporarily wants to return all buttons to the normal framework controller policy.

## Example

```cpp
api->ClearControllerActions(view);
```

## See also

- [Controller actions](../controller-actions.md)
- `BindControllerAction`
- `UnbindControllerAction`
- `Destroy`
