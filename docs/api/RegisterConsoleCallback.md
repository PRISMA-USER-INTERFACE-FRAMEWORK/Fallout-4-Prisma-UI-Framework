# `RegisterConsoleCallback`

**Since:** `IVPrismaUI2`

```cpp
virtual void RegisterConsoleCallback(
    PrismaView view,
    ConsoleMessageCallback callback
) noexcept = 0;
```

Receives every `console.log`/`warn`/`error`/`debug`/`info` call from the view's JS context.

## Parameters

- `view` - Target view handle.
- `callback` - Receives the view, the `ConsoleMessageLevel`, and the message text. Pass `nullptr` to
  unregister.

## Threading

Fires on the **main game thread**.

## Best practice

Register this during development for every view - without it, JS errors are otherwise completely
silent, which makes a broken page look identical to one that just hasn't loaded yet.

## Example

```cpp
api->RegisterConsoleCallback(view,
    [](PrismaView, PRISMA_UI_API::ConsoleMessageLevel lvl, const char* msg) {
        using L = PRISMA_UI_API::ConsoleMessageLevel;
        const char* tag = lvl == L::Error   ? "[JS ERR] "
                        : lvl == L::Warning ? "[JS WARN]"
                                            : "[JS LOG] ";
        logger::info("{} {}", tag, msg);
    });
```
