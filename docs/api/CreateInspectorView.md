# `CreateInspectorView`

**Since:** `IVPrismaUI1`

```cpp
virtual void CreateInspectorView(PrismaView view) noexcept = 0;
```

Retained for V1 ABI/source compatibility.

## Current behavior

The numbered V1 method remains a compatibility entry point and is not the control surface for the current packaged DevTools workflow.

Current PrismaUI builds provide an in-game Ultralight inspector opened with **F12** when DevTools are enabled. Use that inspector for live page inspection, and keep `RegisterConsoleCallback`, `GetViewHealth`, framework logs, and `Invoke` error results available for diagnostics.

The retired external Chrome/legacy-runtime debugging workflow does not apply.

## See also

[`RegisterConsoleCallback`](RegisterConsoleCallback.md), [`GetViewHealth`](GetViewHealth.md), `SetInspectorVisibility`, `IsInspectorVisible`, `SetInspectorBounds`.
