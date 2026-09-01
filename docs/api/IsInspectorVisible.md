# `IsInspectorVisible`

**Since:** `IVPrismaUI1`

```cpp
virtual bool IsInspectorVisible(PrismaView view) noexcept = 0;
```

Retained for V1 ABI/source compatibility.

## PrismaUI_F4 2.1.0 behavior

The Ultralight backend does not implement an inspector UI. For a normal valid 2.1.0 view this compatibility query remains `false`; there is no external Chrome DevTools window whose visibility it tracks.

Do not use this method as a development-mode or backend-health check.

## See also

`CreateInspectorView`, `SetInspectorVisibility`, [`GetViewHealth`](GetViewHealth.md).
