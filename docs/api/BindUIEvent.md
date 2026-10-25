# `BindUIEvent`

**Since:** `IVPrismaUI4`

```cpp
virtual void BindUIEvent(
    PrismaView view,
    const char* functionName,
    JSListenerCallback callback
) noexcept = 0;
```

Same shape as `RegisterJSListener`: exposes a C++ callback to JavaScript as
`window.functionName(arg)`.

## Difference from `RegisterJSListener`

The game-thread guarantee is part of this method's actual contract, not just a detail of the
current backend's implementation. Prefer `BindUIEvent` over `RegisterJSListener` in new code that
needs `RE::*` access - existing `RegisterJSListener` code already gets the same guarantee today, so
there is no need to migrate it, but new code should use the call whose contract you can rely on
going forward.

## Example

```cpp
api->BindUIEvent(view, "requestPlayerName", [](const char* /*arg*/) {
    auto* player = RE::PlayerCharacter::GetSingleton();
    // RE:: access is safe here by contract
});
```

## See also

`RegisterJSListener`.
