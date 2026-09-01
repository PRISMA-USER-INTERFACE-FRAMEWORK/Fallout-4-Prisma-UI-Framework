# View Lifecycle

This page describes the public lifecycle contract for **PrismaUI_F4 2.1.0**. The production backend is in-process **Ultralight 1.4.0**. Older legacy-runtime shell, subprocess, and external-DevTools behavior does not apply to this release.

## States

A normal view moves through these application-visible states:

```text
[not created]
     |
     | CreateView("page.html", onDomReady)
     v
[loading]
     |
     | document becomes ready -> onDomReady is queued to the game thread
     v
[ready + visible]
     |
     | Hide()
     v
[ready + hidden]
     |
     | Show()
     v
[visible + unfocused]
     |
     | Focus(...) or FocusOverlay(...)
     v
[visible + focused]
     |
     | Unfocus()
     v
[visible + unfocused]
     |
     | Hide()
     v
[ready + hidden]
     |
     | Destroy()
     v
[destroyed]
```

`CreateView` returns a `PrismaView` handle immediately. Loading continues asynchronously. Views begin visible, so menus that should not appear immediately normally call `Hide()` after creation.

## Creation

```cpp
PrismaView view = api->CreateView("MyPlugin/page.html", OnDomReady);
if (view != 0) {
    api->Hide(view);
}
```

Create the view when your plugin is ready to own it, commonly from `kPostLoadGame` / `kNewGame` or another bounded game-state event after you have acquired the Prisma API.

```cpp
case F4SE::MessagingInterface::kPostLoadGame:
case F4SE::MessagingInterface::kNewGame:
    if (g_api && (g_view == 0 || !g_api->IsValid(g_view))) {
        g_view = g_api->CreateView("MyPlugin/page.html", OnDomReady);
        if (g_view) {
            g_api->Hide(g_view);
            g_api->RegisterConsoleCallback(g_view, ConsoleCallback);
            g_api->SetViewRole(g_view, PRISMA_UI_API::ViewRole::kPanel);
        }
    }
    break;
```

Do not assume an old handle remains valid across arbitrary world/session transitions. Use `IsValid()` when reusing a stored handle.

## DOM-ready callback

```cpp
static void OnDomReady(PrismaView view)
{
    g_api->RegisterJSListener(view, "onClose", OnClose);
    g_api->RegisterJSListener(view, "requestData", OnDataRequest);

    // RegisterTranslations injects into the current live document in 2.1.0.
    g_api->RegisterTranslations(view, "MyPlugin_F4");

    // Run translation/listener-dependent page initialization after registration.
    g_api->Invoke(view, "window.init && window.init()");
}
```

The framework queues `OnDomReadyCallback` onto the **main game thread**.

For consumer code, registering page-facing listeners in `OnDomReady` is the safest pattern because the document and JS environment are known to exist. The runtime accepts listener registration for a valid view, but registering after DOM readiness avoids depending on backend timing.

`RegisterTranslations` belongs here as well. In 2.1.0 it builds the translation helper script and sends it to the current document through `Invoke`; it is not a pre-document injection hook.

Do not depend on `Invoke()` succeeding before the page has a live JavaScript context.

## Callback threading

In 2.1.0 the public callback paths are marshalled to the game thread:

- `OnDomReadyCallback`
- `JSCallback` returned by `Invoke`
- `JSListenerCallback` registered with `RegisterJSListener`
- `JSListenerCallback` registered with `BindUIEvent`
- `ConsoleMessageCallback`

That means normal `RE::*` access is safe inside those callbacks without adding a second `F4SE::GetTaskInterface()->AddTask` solely to reach the game thread.

```cpp
g_api->RegisterJSListener(view, "queryPlayer", [](const char*) {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;

    const auto hp = player->GetActorValue(RE::ActorValue::kHealth);
    g_api->InteropCall(g_view, "setHealth", std::to_string(hp).c_str());
});
```

If your own worker/network thread calls into game state, you still must dispatch that work appropriately. The guarantee above applies to callbacks delivered by PrismaUI.

## Show, Hide, Focus, and Unfocus

Visibility and focus are separate:

| Operation | Meaning |
|---|---|
| `Show(view)` | Include the view in normal on-screen presentation |
| `Hide(view)` | Stop on-screen presentation while keeping the view alive |
| `Focus(view, ...)` | Give the view normal Prisma input ownership |
| `FocusOverlay(view, ...)` | V10 overlay-focus path used with selective input regions |
| `Unfocus(view)` | Release Prisma input ownership from that view |

Typical panel toggle:

```cpp
void OpenPanel()
{
    if (!g_api || !g_api->IsValid(g_view)) return;

    g_api->SetViewRole(g_view, PRISMA_UI_API::ViewRole::kPanel);
    g_api->Show(g_view);
    g_api->Focus(g_view, true, false);
}

void ClosePanel()
{
    if (!g_api || !g_api->IsValid(g_view)) return;
    g_api->Unfocus(g_view);
    g_api->Hide(g_view);
}
```

Only one Prisma view owns normal framework focus at a time. `GetFocusedView()` identifies it and `HasAnyActiveFocus()` gives the older yes/no form.

## View roles and panel coordination

Any view that behaves as an interactive panel should declare a V10 role:

```cpp
g_api->SetViewRole(g_view, PRISMA_UI_API::ViewRole::kPanel);
```

Passive HUD-style views should use `kWidget`. Undeclared views are not counted as panels by `IsAnyPanelVisible()`, so leaving an interactive panel at `kUnspecified` can cause other Prisma mods to open over it.

Before opening your own panel:

```cpp
if (g_api->IsAnyPanelVisible(g_view)) {
    return;
}
```

## V10 selective input regions

`FocusOverlay` and `SetInputRegions` support interaction with only selected rectangles instead of making the entire view surface an input blocker.

This behavior is capability-gated. Check it first:

```cpp
if (PRISMA_UI_API::HasPrismaCapability(
        PRISMA_UI_API::PrismaCapability::InputRegions)) {
    const PRISMA_UI_API::InputRegion regions[] = {
        { 80, 80, 500, 300 },
    };

    if (g_api->SetInputRegions(g_view, regions, 1)) {
        g_api->FocusOverlay(g_view, false, false);
    }
}
```

See [`FocusOverlay`](api/FocusOverlay.md) and [`SetInputRegions`](api/SetInputRegions.md).

## Escape ownership

`SetViewOwnsEscape(view, true)` opts a focused view into receiving Escape instead of leaving it to the game's normal path.

```cpp
g_api->SetViewOwnsEscape(g_view, true);
```

Then handle it in the page and call your normal close listener:

```js
document.addEventListener('keydown', (event) => {
  if (event.key === 'Escape') {
    event.preventDefault();
    window.onClose();
  }
});
```

Only enable this on a view that actually handles Escape.

## Multiple views

Each Prisma view has its own handle and page state. Do not use the retired shared-legacy-runtime-shell model to reason about 2.1.0.

Ordering is controlled with `SetOrder` / `GetOrder`:

```cpp
api->SetOrder(backgroundView, 0);
api->SetOrder(popupView, 10);
```

For most plugins, prefer one view with internal routing for related screens. Use multiple views for genuinely separate surfaces such as a persistent widget plus an independent panel.

## Hidden and offscreen views

`Hide()` controls normal 2D presentation. V5 offscreen rendering is a separate mode used for texture handoff / mesh-oriented consumers.

Use `SetViewOffscreen(view, true)` and the related V5/V8 APIs only when you specifically need an offscreen texture. Do not confuse offscreen mode with an ordinary hidden panel.

The public 2.1.0 default remains `bMeshBinding=0`; on-mesh behavior should be treated as an explicit advanced integration rather than a requirement for ordinary HTML overlays.

## Health and recovery

V8 exposes `GetViewHealth()` for framework-observed view state:

- `kCreating`
- `kDomReady`
- `kLive`
- `kLoadFailed`
- `kDomReadyTimeout`
- `kUnresponsive`
- `kJsError`

Use it when you need diagnostics or a bounded recovery policy. If a consumer decides a view should be recreated, destroy the old handle and create a new view rather than continuing to call a dead handle.

## Inspector compatibility methods

The V1 inspector methods remain in the ABI, but **the Ultralight 2.1.0 backend does not implement an inspector UI**.

Current behavior:

- `CreateInspectorView` logs that the operation is unsupported.
- `SetInspectorVisibility` logs that the operation is unsupported.
- `IsInspectorVisible` returns false.
- `SetInspectorBounds` is unsupported/no-op.

Do not tell users to enable a legacy-runtime remote-debugging port or expect DevTools to open in an external browser. For 2.1.0 debugging, use `RegisterConsoleCallback`, `PrismaUI_F4.log`, view-health diagnostics, and normal browser development outside the game.

## Scrolling-size compatibility methods

`GetScrollingPixelSize` and `SetScrollingPixelSize` remain in the V1 ABI but are **not implemented by the 2.1.0 Ultralight backend**.

- `GetScrollingPixelSize` returns `0` and logs a warning.
- `SetScrollingPixelSize` leaves the view unchanged and logs a warning.

Normal browser/page scrolling still comes from the page and input events. Do not build new behavior around the V1 pixel-size compatibility calls.

## Destruction

`Destroy()` tears down the view and invalidates its handle.

```cpp
if (g_api && g_api->IsValid(g_view)) {
    g_api->Unfocus(g_view);
    g_api->Hide(g_view);
    g_api->Destroy(g_view);
}
g_view = 0;
```

The framework also clears per-view bookkeeping such as roles, input regions, escape ownership, callbacks, and offscreen tracking when the view is forgotten.

## Related guides

- [Getting Started](getting-started)
- [API Reference](api-reference)
- [Translations](translations)
- [Panel Management](panel-management)
- [View Watchdog](view-watchdog)
- [Limitations](limitations)
- [Troubleshooting](troubleshooting)
