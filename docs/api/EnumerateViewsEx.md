# `EnumerateViewsEx`

**Since:** `IVPrismaUI8`

```cpp
virtual void EnumerateViewsEx(ViewEnumCallbackEx callback, void* userdata) noexcept = 0;
```

Same as `EnumerateViews` (V4), but the callback also reports which plugin created each view.

## Parameters

- `callback` - `ViewEnumCallbackEx`: invoked once per view with its id, `htmlPath`, and `owner` (the
  basename of the module that created the view, e.g. `"MyPlugin_F4.dll"`, resolved by the framework
  from the calling module's own return address at `CreateView` time - real module identity, not a
  self-reported string a plugin could get wrong or spoof). `owner` is an empty string if it could
  not be resolved, or the view predates ownership tracking.
- `userdata` - Passed through unchanged to every callback invocation.

## Thread safety

Safe to call from any thread, same as `EnumerateViews`.

## See also

`EnumerateViews` (V4) - the version without owner information, kept for existing callers.
