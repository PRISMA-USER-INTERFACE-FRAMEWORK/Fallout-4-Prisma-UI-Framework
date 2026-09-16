# `SetScrollingPixelSize`

**Since:** `IVPrismaUI1`

```cpp
virtual void SetScrollingPixelSize(PrismaView view, int pixelSize) noexcept = 0;
```

Retained for V1 ABI/source compatibility.

## PrismaUI_F4 2.1.0 behavior

This method is **not implemented by the Ultralight 1.4.0 backend**. The requested value is ignored, the view is unchanged, and the framework logs a warning.

Do not use it to tune DOM scrolling in new 2.1.0 code. Implement page scrolling behavior in your HTML/JavaScript/CSS instead.

## See also

[`GetScrollingPixelSize`](GetScrollingPixelSize.md).
