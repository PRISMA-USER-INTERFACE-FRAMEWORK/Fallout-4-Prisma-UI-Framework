# `SetControllerStyle`

**Since:** `IVPrismaUI9`

```cpp
virtual void SetControllerStyle(int style) noexcept = 0;
```

Sets the controller art style: `0` for Xbox, `1` for PlayStation. Purely a display toggle for
button art/glyphs - it does not change how input is read.

Writes `PrismaUI_F4.ini`'s `[Controller] Style` immediately, so the change is both persisted and
visible to every other Prisma plugin right away.

## See also

`GetControllerStyle`.
