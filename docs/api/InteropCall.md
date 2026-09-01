# `InteropCall`

**Since:** `IVPrismaUI1`

```cpp
virtual void InteropCall(
    PrismaView view,
    const char* functionName,
    const char* argument
) noexcept = 0;
```

Calls a named JavaScript function via the JS Interop API. Lower overhead than `Invoke`, and the
right default for anything called more than once per second, or in a tight loop.

## Parameters

- `view` - Target view handle.
- `functionName` - Name of a function that must exist on the page's `window` object.
- `argument` - A single string argument. Pass JSON if you need to send structured data.

## Encoding

Same ANSI -> UTF-8 auto-conversion as `Invoke`.

## Example

```cpp
// C++
api->InteropCall(view, "onInventoryData", jsonString.c_str());
```

```javascript
// JS - mymenu.html
function onInventoryData(json) {
  const items = JSON.parse(json);
  // render items...
}
```

## See also

`Invoke` - for one-shot calls where you need a return value back in C++.
