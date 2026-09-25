# `SetInspectorVisibility`

**Since:** `IVPrismaUI1`

```cpp
virtual void SetInspectorVisibility(PrismaView view, bool visible) noexcept = 0;
```

Retained for V1 ABI/source compatibility.

## Current behavior

This V1 method is retained for ABI compatibility and should not be used to control the current DevTools session.

Current PrismaUI builds package an in-game Ultralight inspector opened with **F12** when DevTools are enabled. That F12 workflow is separate from the historical inspector visibility API.

## See also

`CreateInspectorView`, `IsInspectorVisible`, `SetInspectorBounds`, [`RegisterConsoleCallback`](RegisterConsoleCallback.md).
