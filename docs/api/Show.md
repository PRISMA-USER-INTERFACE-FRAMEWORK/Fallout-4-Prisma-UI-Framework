# `Show`

**Since:** `IVPrismaUI1`

```cpp
virtual void Show(PrismaView view) noexcept = 0;
```

Makes a hidden view visible at the next composite pass. Does not grant input focus - call `Focus`
separately if the view should also receive input.

## See also

`Hide`, `IsHidden`, `Focus`.

**Try it:** [Build your first panel](../quick-start).
