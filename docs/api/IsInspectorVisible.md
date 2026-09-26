# `IsInspectorVisible`

**Since:** `IVPrismaUI1`

```cpp
virtual bool IsInspectorVisible(PrismaView view) noexcept = 0;
```

Retained for V1 ABI/source compatibility.

## Current behavior

Do not use this V1 compatibility query as the authoritative state of the current F12 Ultralight inspector.

Current builds provide the packaged in-game inspector when DevTools are enabled. Treat this method as compatibility surface rather than a development-mode or backend-health check.

## See also

`CreateInspectorView`, `SetInspectorVisibility`, [`GetViewHealth`](GetViewHealth.md).
