# `CreateInspectorView`

**Since:** `IVPrismaUI1`

```cpp
virtual void CreateInspectorView(PrismaView view) noexcept = 0;
```

Retained for V1 ABI/source compatibility.

## PrismaUI_F4 2.1.0 behavior

The production **Ultralight 1.4.0** backend does not implement an inspector UI. This call logs that the operation is unsupported and does not create Chrome DevTools, an external browser session, or an in-game inspector.

For 2.1.0 debugging, use `RegisterConsoleCallback`, `GetViewHealth`, `Invoke` error results, your plugin log, and `PrismaUI_F4.log`.

Existing code may leave this compatibility call in place, but new code should not depend on it.

## See also

[`RegisterConsoleCallback`](RegisterConsoleCallback.md), [`GetViewHealth`](GetViewHealth.md), `SetInspectorVisibility`, `IsInspectorVisible`, `SetInspectorBounds`.
