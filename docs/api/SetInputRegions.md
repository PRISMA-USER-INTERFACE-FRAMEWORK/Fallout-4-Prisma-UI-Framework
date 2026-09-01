# `SetInputRegions`

**Since:** `IVPrismaUI10`

```cpp
struct InputRegion {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
};

virtual bool SetInputRegions(
    PrismaView view,
    const InputRegion* regions,
    uint32_t count
) noexcept = 0;
```

Defines the screen-space rectangles that should be treated as interactive for a V10 overlay-focused view.

This is intended for overlays that are mostly click-through but contain specific buttons, cards, or panels that should receive Prisma input.

## Parameters

- `view` - Valid Prisma view handle.
- `regions` - Pointer to an array of `InputRegion` rectangles.
- `count` - Number of rectangles in the array.

## Returns

Returns `true` when the region update is accepted. Returns `false` when the view is invalid or the provider does not expose the required input-region capability.

## Capability check

Always check the capability before making this feature mandatory:

```cpp
if (!PRISMA_UI_API::HasPrismaCapability(
        PRISMA_UI_API::PrismaCapability::InputRegions)) {
    return;
}
```

## Example

```cpp
PRISMA_UI_API::InputRegion regions[] = {
    { 40, 40, 320, 80 },
    { 40, 140, 480, 300 },
};

if (PRISMA_UI_API::HasPrismaCapability(
        PRISMA_UI_API::PrismaCapability::InputRegions)) {
    api->SetInputRegions(view, regions, 2);
    api->FocusOverlay(view, false, false);
}
```

Keep the regions synchronized with your actual layout. A stale rectangle can make an apparently empty area consume input or make a visible control click-through to the game.

## See also

- `FocusOverlay`
- `Focus`
- `SetViewRole`
