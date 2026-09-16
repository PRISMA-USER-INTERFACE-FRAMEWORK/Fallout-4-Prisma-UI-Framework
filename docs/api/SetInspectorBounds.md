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

## PrismaUI_F4 2.1.0 behavior

The Ultralight backend has no inspector UI, so these bounds have nothing to position. The parameters are ignored and the call has no presentation effect.

Do not use this as evidence that an inspector exists. Use console capture and framework logging for 2.1.0 debugging.

## See also

`CreateInspectorView`, `SetInspectorVisibility`, [`RegisterConsoleCallback`](RegisterConsoleCallback.md).
