---
id: view-watchdog
title: View health and watchdog
sidebar_label: View health
sidebar_position: 9
---

:::tip New to PrismaUI?
Start with [Your first panel](quick-start) for a complete walkthrough, or copy a working example from the [Cookbook](/docs/cookbook/).
:::

# View health and watchdog

`GetViewHealth(view)` tells you what the framework thinks of a view: still loading, live, failed, timed out, unresponsive, or hit a JavaScript error. It is a diagnostic tool with a small recovery pattern, not a state machine you should build your plugin around.

The watchdog is the framework's own safety net for focused views: if presentation stalls, it releases focus and pause so the player is never stuck.

## The states

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

| State | What it means | What to do |
| --- | --- | --- |
| `kUnknown` | The framework cannot report health for this handle. | Check `IsValid(view)` separately. |
| `kCreating` | Created, not ready yet. | Do not focus. Wait for DOM-ready. |
| `kDomReady` | The page DOM exists and PrismaUI published that state. | Safe to focus and call the bridge. |
| `kLive` | Normal load path completed. | Normal use. |
| `kLoadFailed` | The page failed to load, or navigation was rejected. | Treat as a path/asset problem, not a random failure. |
| `kDomReadyTimeout` | The page never reached DOM-ready in the allowed window. | Make a bounded recreate attempt, capture evidence. |
| `kUnresponsive` | The view stopped responding. | Log it; recreate with a bound. |
| `kJsError` | A JavaScript error put the view in its error state. | Fix the page; `Invoke` errors land here. |

Focus is only accepted when the view is `kDomReady` or `kLive`. That is why a panel created one frame ago refuses `Focus`.

## Reading it

```cpp
if (!g_api || !g_api->IsValid(g_view)) return;

const auto health = g_api->GetViewHealth(g_view);
```

Health is cheap but not free; check it when something goes wrong or at low frequency. Do not poll it every frame for a normal menu.

```cpp
using VH = PRISMA_UI_API::ViewHealth;

switch (g_api->GetViewHealth(g_view)) {
case VH::kDomReady:
case VH::kLive:
    break;

case VH::kCreating:
    break;   // not a failure: still loading

case VH::kLoadFailed:
case VH::kDomReadyTimeout:
    REX::ERROR("view failed to become live");
    break;

case VH::kUnresponsive:
    REX::ERROR("view became unresponsive");
    break;

case VH::kJsError:
    REX::WARN("view has a JavaScript error");
    break;

default:
    break;
}
```

## Finding the cause

Health tells you *that* something broke. These tell you *what*:

- **`RegisterConsoleCallback`** — the in-game console, including your page's errors, in your log.
- **`PrismaUI_F4.log`** — framework-level: rejected navigation, preflight failures, JS errors, focus failures.
- **F12 inspector** — Elements, Console, Network when a view has focus.
- **`Invoke` errors** — an exception from an `Invoke` often shows up as `kJsError`.

## The watchdog

You do not need to feed a focused static view fake animation. The presentation heartbeat is refreshed when the focused view is part of the successfully drawn frames — a static panel stays healthy.

Emergency focus release exists for a real whole-frame presentation stall, not for a single dropped frame. When it fires, the player gets input and pause control back instead of being trapped in a view that is not being drawn.

That is the point of the watchdog: protect the player, not the plugin.

## Recovering a broken view

Destroy and recreate — with a limit:

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

What you should **not** do is recreate on every update. If the HTML path is wrong, an asset is missing or the page throws during startup, recreating just repeats the failure — and now the player sees flicker on top of it.

A sane policy:

1. recreate once per failure;
2. if the same view fails again quickly, stop and log;
3. investigate the log before adding retries.

## What changed from older watchdog guides

- There is no renderer subprocess and no legacy-runtime crash log to read. PrismaUI renders in-process.
- Do not investigate old host/iframe/shell failures; they do not exist in current builds.
- Debugging is the framework log, `RegisterConsoleCallback`, `Invoke` errors, `IsValid`, `GetViewHealth`, and the F12 inspector.

## Where to go next

- [Debugging your view](debugging) — the full debugging path
- [View lifecycle](view-lifecycle) — states and destruction in order
- [`GetViewHealth`](api/GetViewHealth) — the method page
- [Troubleshooting](troubleshooting) — symptom-driven checks
