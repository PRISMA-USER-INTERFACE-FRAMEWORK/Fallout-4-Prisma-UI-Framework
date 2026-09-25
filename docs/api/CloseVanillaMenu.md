# `CloseVanillaMenu`

**Since:** `IVPrismaUI6`

```cpp
virtual bool CloseVanillaMenu(const char* menuName) noexcept = 0;
```

Closes a vanilla menu immediately, by `MENU_NAME`.

This is a one-shot action - it does not affect whether that menu can open again in the future.
Combine with `SuppressVanillaMenu(name, true)` first if you also want it to stay closed going
forward.

## See also

`SuppressVanillaMenu`.
