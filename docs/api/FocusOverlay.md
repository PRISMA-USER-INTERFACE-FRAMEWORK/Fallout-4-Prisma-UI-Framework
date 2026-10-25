# `FocusOverlay`

**Since:** `IVPrismaUI10`

```cpp
virtual bool FocusOverlay(
    PrismaView view,
    bool pauseGame = false,
    bool disableFocusMenu = false
) noexcept = 0;
```

Focuses a view using the V10 overlay/input-region path. Use this when an overlay should receive input selectively instead of behaving like a conventional full interactive panel.

## Parameters

- `view` - Valid Prisma view handle.
- `pauseGame` - Whether the framework should pause the game while this overlay owns focus.
- `disableFocusMenu` - Whether to disable the normal FocusMenu integration for this focus operation.

## Returns

Returns `true` when the overlay focus request is accepted. Returns `false` for an invalid view or when the capability required by the installed provider is unavailable.

## Capability check

Input-region behavior is capability-gated. Before making overlay input regions a hard dependency, check:

```cpp
if (!PRISMA_UI_API::HasPrismaCapability(
        PRISMA_UI_API::PrismaCapability::InputRegions)) {
    // Fall back to normal Focus(), or disable the selective-input feature.
}
```

`RequestPluginAPI<IVPrismaUI10>()` proves that the V10 interface exists. `HasPrismaCapability(InputRegions)` proves that the optional input-region behavior is actually enabled by the loaded provider.

## Typical use

```cpp
auto* api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI10>();
if (!api || !api->IsValid(view)) return;

api->Show(view);
if (PRISMA_UI_API::HasPrismaCapability(PRISMA_UI_API::PrismaCapability::InputRegions)) {
    api->FocusOverlay(view, false, false);
} else {
    api->Focus(view, false, false);
}
```

## See also

- `Focus`
- `SetInputRegions`
- `SetViewRole`
- `GetFocusedView`
