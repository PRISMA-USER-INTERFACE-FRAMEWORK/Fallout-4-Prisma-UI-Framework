# `GetViewHealth`

**Since:** `IVPrismaUI8`

```cpp
virtual ViewHealth GetViewHealth(PrismaView view) noexcept = 0;
```

Returns the framework-published health state for a view. Use it with `IsValid()` and logging when diagnosing lifecycle failures.

## Public values

| Value | Meaning |
|---|---|
| `kUnknown` | No known public health state for the supplied handle/backend state. |
| `kCreating` | View admitted/created but not yet DOM-ready/live. |
| `kDomReady` | Main document reached DOM-ready. |
| `kLive` | Normal live state. |
| `kLoadFailed` | View/document loading failed or trusted-navigation policy rejected the document. |
| `kDomReadyTimeout` | ABI health state for failing to reach DOM-ready in the allowed lifecycle window. |
| `kUnresponsive` | ABI health state for an unresponsive view condition. |
| `kJsError` | JavaScript/script error condition, for example a failed evaluated script. |

## PrismaUI_F4 2.1.0 notes

The production backend is in-process Ultralight. Do not interpret `kUnresponsive` as proof that a legacy-runtime renderer subprocess crashed.

The current backend publishes `kCreating`, `kDomReady`, `kLive`, `kLoadFailed`, and script-error state as the Ultralight view moves through creation/loading/evaluation. The public enum retains the full compatibility state set.

Focus is accepted only when the view is in `kDomReady` or `kLive`.

## Example

```cpp
if (!g_api || !g_api->IsValid(g_view)) {
    return;
}

using VH = PRISMA_UI_API::ViewHealth;
const auto health = g_api->GetViewHealth(g_view);

if (health == VH::kLoadFailed || health == VH::kDomReadyTimeout) {
    logger::error("Prisma view failed to become live");
}
```

Use `RegisterConsoleCallback`, `Invoke` error results, and `PrismaUI_F4.log` to determine the actual cause.

## See also

[`IsValid`](IsValid.md), [`RegisterConsoleCallback`](RegisterConsoleCallback.md), [View Health & Watchdog](../view-watchdog.md).
