# `IsGameThread`

**Since:** `IVPrismaUI11`

```cpp
virtual bool IsGameThread() noexcept = 0;
```

Returns whether the caller is currently executing on PrismaUI's verified owner thread for the Fallout window.

## Returns

Returns `true` only after the Fallout HWND owner thread has been verified and the current call is executing on that exact thread.

The public method name is `IsGameThread`, but the concrete guarantee is the verified Fallout window thread. It should not be treated as independent proof of every engine subsystem's simulation-thread identity.

## Typical use

```cpp
if (api->IsGameThread()) {
    // Already on the verified Fallout window thread.
} else {
    api->DispatchToGameThread(MyCallback, userdata);
}
```

Remember that `DispatchToGameThread` is intentionally deferred even when called from the verified thread.

## Provider boundary

V11 is flat Fallout 4 only. The VR provider does not advertise V11.

## See also

- [Current API extensions](../api-extensions.md)
- `DispatchToGameThread`
- `BindGameThreadUIEvent`
