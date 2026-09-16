# `GetScrollingPixelSize`

**Since:** `IVPrismaUI1`

```cpp
virtual int GetScrollingPixelSize(PrismaView view) noexcept = 0;
```

Retained for V1 ABI/source compatibility.

## PrismaUI_F4 2.1.0 behavior

This method is **not implemented by the Ultralight 1.4.0 backend**. For a nonzero view it logs a warning and returns `0`.

Do not treat `0` as the view's actual DOM scroll amount or wheel-step configuration. Normal page scrolling is handled by the page/input path; this compatibility getter does not expose it.

## See also

[`SetScrollingPixelSize`](SetScrollingPixelSize.md).
