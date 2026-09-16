# `SetInspectorVisibility`

**Since:** `IVPrismaUI1`

```cpp
virtual void SetInspectorVisibility(PrismaView view, bool visible) noexcept = 0;
```

Retained for V1 ABI/source compatibility.

## PrismaUI_F4 2.1.0 behavior

The Ultralight 1.4.0 backend does not expose the old inspector/DevTools UI. This call clears any compatibility visibility bookkeeping, logs that inspector visibility is unsupported, and does not open an external browser or in-game inspector.

There is no `[DevTools] bEnabled=1` setup for the released 2.1.0 backend.

Use `RegisterConsoleCallback`, `GetViewHealth`, `Invoke` error results, and framework logs for debugging.

## See also

`CreateInspectorView`, `IsInspectorVisible`, `SetInspectorBounds`, [`RegisterConsoleCallback`](RegisterConsoleCallback.md).
