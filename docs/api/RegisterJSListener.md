# `RegisterJSListener`

**Since:** `IVPrismaUI1`

```cpp
virtual void RegisterJSListener(
    PrismaView view,
    const char* functionName,
    JSListenerCallback callback
) noexcept = 0;
```

Exposes a C++ callback to JavaScript. After registration, calling `window.functionName(arg)` from
JS invokes the C++ callback.

## Parameters

- `view` - Target view handle.
- `functionName` - The name JS will call as `window.functionName(arg)`.
- `callback` - Receives the single string argument passed from JS.

## Threading

Dispatched onto the **main game thread** - the framework wraps every `RegisterJSListener` callback
in its own `F4SE::GetTaskInterface()->AddTask` before it reaches your code. `RE::*` access,
`InteropCall`/`Invoke` calls, and any other game-thread work are safe to do directly inside the
callback; no manual dispatch is needed.

`BindUIEvent` (V4) makes the same guarantee as part of the API's actual contract rather than as a
detail of the current backend - prefer it for new code, but existing `RegisterJSListener` usage is
not a bug and does not need to change.

## Best practice

Register listeners inside your `OnDomReadyCallback` (see `CreateView`), so they exist before any JS
on the page could call them.

## Example

```cpp
// C++ - register in OnDomReady
api->RegisterJSListener(view, "onCloseRequest", [](const char* /*arg*/) {
    g_api->Unfocus(g_view);
    g_api->Hide(g_view);
});
```

```javascript
// JS - call from the page
onCloseRequest();
```

## See also

`BindUIEvent` (V4), `InteropCall`, `Invoke`.
