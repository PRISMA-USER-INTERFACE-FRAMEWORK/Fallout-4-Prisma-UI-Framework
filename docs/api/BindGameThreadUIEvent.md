# `BindGameThreadUIEvent`

**Since:** `IVPrismaUI11`

```cpp
virtual bool BindGameThreadUIEvent(
    PrismaView view,
    const char* functionName,
    GameThreadUIEventCallback callback,
    void* userdata
) noexcept = 0;
```

Registers a JavaScript-to-native event whose native callback is delivered on PrismaUI's verified Fallout window thread.

Use this for page actions that need to touch Fallout-owned state, menus, Scaleform, or other engine objects.

## Parameters

- `view` - Live Prisma view handle.
- `functionName` - JavaScript bridge function/event name.
- `callback` - Native callback receiving the serialized argument and `userdata`.
- `userdata` - Consumer-owned context pointer.

## Returns

Returns `true` when the binding is accepted. Returns `false` when the dispatcher is not ready or the registration cannot be established.

## Lifetime

Keep `userdata` valid until `Destroy(view)` returns. Destroy waits out an in-flight callback when called from another thread, but a callback that calls `Destroy(view)` itself may still be executing when `Destroy` returns.

Do not call `Destroy` while holding a lock that the bound callback may need.

## Example

```cpp
static void OnApply(const char* argument, void* userdata) {
    auto* state = static_cast<MyState*>(userdata);
    // Safe place for the verified Fallout-window-thread mutation.
}

if (!api->BindGameThreadUIEvent(view, "applySetting", OnApply, &g_state)) {
    logger::warn("Could not register applySetting");
}
```

## Provider boundary

V11 is flat Fallout 4 only. The VR provider does not advertise V11.

## See also

- [Current API extensions](../api-extensions.md)
- `DispatchToGameThread`
- `IsGameThread`
- [Controller actions](../controller-actions.md)
