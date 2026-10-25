# `IsValid`

**Since:** `IVPrismaUI1`

```cpp
virtual bool IsValid(PrismaView view) noexcept = 0;
```

Returns `true` if the view handle is live and backed by a real browser instance.

Check this before any operation on a stored handle you haven't used recently, particularly after a
game reload - a `PrismaView` handle from a previous game session is not valid in the new one.

## See also

`Destroy`, `GetViewHealth` (V8) - a richer health check beyond simple validity.
