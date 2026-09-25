# `SetInspectorBounds`

**Since:** `IVPrismaUI1`

```cpp
virtual void SetInspectorBounds(
    PrismaView view,
    float topLeftX,
    float topLeftY,
    unsigned int width,
    unsigned int height
) noexcept = 0;
```

Retained for V1 ABI/source compatibility.

## Current behavior

This V1 compatibility method is not the layout control for the current packaged DevTools inspector.

Current builds open the in-game Ultralight inspector with **F12** when DevTools are enabled. Use the inspector's own UI rather than this legacy bounds method.

## See also

`CreateInspectorView`, `SetInspectorVisibility`, [`RegisterConsoleCallback`](RegisterConsoleCallback.md).
