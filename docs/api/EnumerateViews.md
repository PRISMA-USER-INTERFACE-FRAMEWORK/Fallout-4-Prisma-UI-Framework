# `EnumerateViews`

**Since:** `IVPrismaUI4`

```cpp
virtual void EnumerateViews(ViewEnumCallback callback, void* userdata) noexcept = 0;
```

Enumerates every currently-registered view across all plugins, synchronously, from any thread.

## Parameters

- `callback` - Invoked once per view with its `PrismaView` id and the relative `htmlPath` it was
  created with.
- `userdata` - Passed through unchanged to every callback invocation, for context without needing a
  capture-owning lambda.

## Superseded by

`EnumerateViewsEx` (V8), which adds the creating plugin's module name per view. Prefer that for new
code; this method is kept for existing callers.

## See also

`EnumerateViewsEx`.
