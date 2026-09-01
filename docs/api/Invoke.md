# `Invoke`

**Since:** `IVPrismaUI1`

```cpp
virtual void Invoke(
    PrismaView view,
    const char* script,
    JSCallback callback = nullptr
) noexcept = 0;
```

Evaluates an arbitrary JavaScript expression in the view's context.

## Parameters

- `view` - Target view handle.
- `script` - Any JS expression or statement, evaluated as if typed into the page's console.
- `callback` - Optional. Receives the expression's result, converted to a string, on the
  **main game thread**.

## Performance

Higher overhead than `InteropCall` - it posts to the browser's render process and waits for a
round-trip. Use `Invoke` for one-shot calls and reads, not for anything called more than once per
second or in a tight loop; use `InteropCall` for that instead.

## Encoding

Auto-converts ANSI game strings to UTF-8.

## Example

```cpp
// Push data into the page
api->Invoke(view, "updateInventory('[{\"name\":\"Stimpack\",\"count\":5}]')");

// Read a value back
api->Invoke(view, "document.getElementById('hp').textContent", [](const char* val) {
    logger::info("HP display: {}", val);
});
```

## See also

`InteropCall` - lower-overhead one-way call for anything frequent.
