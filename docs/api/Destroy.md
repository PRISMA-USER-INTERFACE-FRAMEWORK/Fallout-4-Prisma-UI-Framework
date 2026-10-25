# `Destroy`

**Since:** `IVPrismaUI1`

```cpp
virtual void Destroy(PrismaView view) noexcept = 0;
```

Tears down the view completely. The handle becomes invalid immediately after this call. Rarely
needed in practice - views are typically kept alive (hidden when not in use) for the whole game
session rather than destroyed and recreated.

## See also

`IsValid`, `Hide`.
