---
id: view-watchdog
title: View Health & Watchdog
sidebar_label: View Health & Watchdog
sidebar_position: 9
---

# View Health & Watchdog

`IVPrismaUI8::GetViewHealth` exposes framework-observed health for a Prisma view. In **PrismaUI_F4 2.1.0** those observations come from the in-process **Ultralight 1.4.0** backend, not from a legacy-runtime renderer subprocess.

Use view health for diagnostics and bounded recovery. Do not treat it as a substitute for `IsValid()` or build an infinite recreate loop around it.

## Public states

The ABI keeps these values:

```cpp
enum class ViewHealth : int {
    kUnknown         = -1,
    kCreating        =  0,
    kDomReady        =  1,
    kLive            =  2,
    kLoadFailed      =  3,
    kDomReadyTimeout =  4,
    kUnresponsive    =  5,
    kJsError         =  6,
};
```

### `kUnknown`

The public API returns this when it cannot provide a known health state for the supplied handle/backend state.

Check `IsValid(view)` separately when the distinction matters.

### `kCreating`

The view has been admitted/created but has not yet reached the DOM-ready/live state.

Do not focus a view in this state. The 2.1.0 focus path rejects a view unless its health is `kDomReady` or `kLive`.

### `kDomReady`

The page has reached DOM-ready and the framework has published that state. Consumer DOM-ready callbacks are queued to the game thread.

This is a valid state for focus and normal JS bridge work.

### `kLive`

The view completed the normal load path and is live.

### `kLoadFailed`

The current page/view load failed or the framework rejected the main document/navigation. Examples include a missing/invalid view, failed Ultralight load, or an attempted top-level navigation outside the trusted document boundary.

Treat repeated `kLoadFailed` as a configuration/content problem until proven otherwise. Check `PrismaUI_F4.log`, view paths, and bundled assets before automatically recreating forever.

### `kDomReadyTimeout`

Retained in the public health ABI for a page that fails to reach DOM-ready within the framework's allowed lifecycle window.

If it is reported, capture console/log evidence and recreate only with a bounded retry policy.

### `kUnresponsive`

Retained in the public health ABI for an unresponsive view condition.

Do **not** interpret this as “the legacy-runtime subprocess crashed.” PrismaUI 2.1.0 is in-process Ultralight. Log the state and collect framework diagnostics before deciding whether to recreate.

### `kJsError`

The public representation of a JavaScript/script-error health condition. For example, an `Invoke` evaluation exception can move the backend into its script-error state.

Use `RegisterConsoleCallback` and the `Invoke` result to diagnose the actual exception.

## Querying health

```cpp
if (!g_api || !g_view || !g_api->IsValid(g_view)) {
    return;
}

const auto health = g_api->GetViewHealth(g_view);
```

Health is most useful at low frequency or when you already have a reason to diagnose a view. There is no need to poll it every render frame for a normal menu.

A simple bounded check:

```cpp
using VH = PRISMA_UI_API::ViewHealth;

switch (g_api->GetViewHealth(g_view)) {
case VH::kDomReady:
case VH::kLive:
    break;

case VH::kCreating:
    // Still starting. Do not focus yet.
    break;

case VH::kLoadFailed:
case VH::kDomReadyTimeout:
    logger::error("Prisma view failed to become live");
    break;

case VH::kUnresponsive:
    logger::error("Prisma view became unresponsive");
    break;

case VH::kJsError:
    logger::warn("Prisma view reported a JavaScript error");
    break;

case VH::kUnknown:
default:
    break;
}
```

## Console diagnostics

`RegisterConsoleCallback` is the primary in-game replacement for old legacy-runtime DevTools-oriented debugging instructions.

```cpp
g_api->RegisterConsoleCallback(g_view,
    [](PrismaView view,
       PRISMA_UI_API::ConsoleMessageLevel level,
       const char* message) {
        logger::info("[Prisma JS][{}][{}] {}",
                     view,
                     static_cast<int>(level),
                     message ? message : "");
    });
```

Console callbacks are delivered on the game thread in 2.1.0.

Also inspect:

```text
Documents\My Games\Fallout4\F4SE\PrismaUI_F4.log
```

The framework records rejected navigation, runtime preflight failures, JS errors, focus failures, and other lifecycle diagnostics there.

## Recovery pattern

A failed handle can be destroyed and recreated, but keep retries bounded:

```cpp
void RecreateView()
{
    if (!g_api) return;

    if (g_view && g_api->IsValid(g_view)) {
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
        g_api->Destroy(g_view);
    }
    g_view = 0;

    g_view = g_api->CreateView("MyPlugin/index.html", OnDomReady);
    if (g_view) {
        g_api->SetViewRole(g_view, PRISMA_UI_API::ViewRole::kPanel);
        g_api->Hide(g_view);
    }
}
```

Do not immediately recreate forever on every update. A bad HTML path, missing asset, blocked navigation, or repeatable JS startup error will simply reproduce the same failure.

## Important 2.1.0 differences from older watchdog docs

- There is no production legacy-runtime renderer subprocess to diagnose.
- Do not tell users to inspect legacy-runtime crash logs.
- Do not describe every view as an iframe in a shared legacy-runtime shell.
- Do not rely on external previous browser runtime DevTools. The V1 inspector API is unsupported under Ultralight 2.1.0.
- Use framework logs, `RegisterConsoleCallback`, `Invoke` errors, `IsValid`, and `GetViewHealth` instead.

## Related pages

- [View Lifecycle](view-lifecycle)
- [Troubleshooting](troubleshooting)
- [API Reference](api-reference)
- [`GetViewHealth`](api/GetViewHealth.md)
