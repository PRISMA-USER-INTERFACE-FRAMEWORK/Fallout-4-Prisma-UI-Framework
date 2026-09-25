# `Hide`

**Since:** `IVPrismaUI1`

```cpp
virtual void Hide(PrismaView view) noexcept = 0;
```

Removes a visible view from the composite. Does not destroy the view or stop its JavaScript from
running - a hidden view's page keeps executing, timers keep firing, and `InteropCall`/`Invoke` keep
working. Use `Destroy` if you actually want to tear the view down.

## See also

`Show`, `IsHidden`, `Destroy`.
