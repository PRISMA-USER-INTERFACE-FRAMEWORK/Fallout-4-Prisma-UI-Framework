# What happens to a view

A PrismaUI view is created, loaded, shown, hidden, and eventually destroyed. This page is the map: what each state means, which calls are safe when, and the mistakes that cause blank panels or dead handles.

If you have not built one yet, do [Your first panel](quick-start) first.

## The short version

```cpp
// Create once, when the game has a world.
g_view = g_api->CreateView("MyPlugin/page.html", OnDomReady);
if (g_view) {
    g_api->SetViewRole(g_view, PRISMA_UI_API::ViewRole::kPanel);
    g_api->Hide(g_view);            // views start visible; hide until opened
}

// Open.
if (g_api->IsValid(g_view)) {
    g_api->Show(g_view);
    g_api->Focus(g_view, /*pauseGame=*/true);
}

// Close.
if (g_api->IsValid(g_view)) {
    g_api->Unfocus(g_view);
    g_api->Hide(g_view);
}

// Tear down (usually only on a new game or when the plugin unloads).
if (g_api->IsValid(g_view)) {
    g_api->Unfocus(g_view);
    g_api->Hide(g_view);
    g_api->Destroy(g_view);
}
g_view = 0;
```

That is all you need for most mods. The rest of this page explains why.

## The states

```
not created
   │ CreateView(path, OnDomReady)
   ▼
loading ────────────────► load failed / timed out
   │ page DOM ready
   ▼
ready, visible
   │ Hide()                     ▲ Show()
   ▼                            │
ready, hidden ───────────────── ┘
   │ Focus()
   ▼
focused (input goes to the page)
   │ Unfocus()
   ▼
visible, unfocused
   │ Destroy()
   ▼
destroyed (handle is no longer valid)
```

Two things surprise people:

- **A view is visible the moment it is created.** If it is a menu, call `Hide()` right after `CreateView` or the player sees it as soon as the plugin creates it.
- **Visible is not focused.** A visible view is drawn; a focused view receives mouse and keyboard input. `Show()` alone gives you a picture with a dead mouse.

## Creating it at the right time

Create the view after the game world exists, and after you have the Prisma API:

```cpp
static void OnMessage(F4SE::MessagingInterface::Message* message)
{
    switch (message->type) {
    case F4SE::MessagingInterface::kGameDataReady:
        g_api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI10>();
        break;

    case F4SE::MessagingInterface::kPostLoadGame:
    case F4SE::MessagingInterface::kNewGame:
        CreateView();
        break;

    default:
        break;
    }
}
```

Do not hold a view handle forever without checking it. If the player loads a save, starts a new game or hops between worlds, re-check `IsValid(g_view)` and recreate if it died:

```cpp
static void EnsureView()
{
    if (g_view && g_api->IsValid(g_view)) return;

    g_view = g_api->CreateView("MyPlugin/page.html", OnDomReady);
    if (g_view) {
        g_api->SetViewRole(g_view, PRISMA_UI_API::ViewRole::kPanel);
        g_api->Hide(g_view);
    }
}
```

## The DOM-ready callback

`OnDomReady` runs when the page's DOM exists and JavaScript is available. Run your page setup there — not earlier:

```cpp
static void OnDomReady(PrismaView view)
{
    // Console output first, so page errors reach your log.
    g_api->RegisterConsoleCallback(view, ConsoleCallback);

    // Page → plugin calls.
    g_api->BindUIEvent(view, "requestClose", [](const char*) {
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
    });
    g_api->BindUIEvent(view, "requestData", [](const char*) {
        SendPlayerData();
    });

    // Translations, controller actions, initial data.
    g_api->RegisterTranslations(view, "MyPlugin_F4");
    g_api->Invoke(view, "window.init && window.init()");
}
```

Each callback PrismaUI delivers is already on the game thread. `RE::` calls are safe inside them, and `InteropCall`/`Invoke` from them are safe too.

If your own worker thread wants to touch the game or the view, post the work back to the game thread yourself. The guarantee only covers callbacks PrismaUI hands you.

## Show, hide, focus, unfocus

These four calls are separate on purpose:

| Call | What it changes |
| --- | --- |
| `Show(view)` | The view is drawn. |
| `Hide(view)` | The view is not drawn; it stays loaded. |
| `Focus(view, pauseGame)` | The view receives mouse and keyboard input. |
| `Unfocus(view)` | Input goes back to the game. |

A typical menu toggle:

```cpp
static void ToggleMenu()
{
    if (!g_api || !g_api->IsValid(g_view)) return;

    g_visible = !g_visible;
    if (g_visible) {
        g_api->Show(g_view);
        g_api->Focus(g_view, /*pauseGame=*/true);
    } else {
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
    }
}
```

Only one Prisma view owns normal focus at a time. `GetFocusedView()` returns it; `HasAnyActiveFocus()` is the older yes/no call.

## Telling other mods what your view is

Declare a role once, right after creating an interactive view:

```cpp
g_api->SetViewRole(g_view, PRISMA_UI_API::ViewRole::kPanel);   // takes input, counts as a panel
g_api->SetViewRole(g_hud,  PRISMA_UI_API::ViewRole::kWidget);  // passive HUD
```

- `kPanel` means "this is an interactive screen". Other cooperating Prisma mods check `IsAnyPanelVisible()` and wait instead of opening on top of you.
- `kWidget` means "always on screen, does not block anything".
- `kUnspecified` (the default) is invisible to those checks. Leaving a real menu at `kUnspecified` makes your panel look absent to everyone else.

To wait politely before opening:

```cpp
if (g_api->IsAnyPanelVisible(g_view)) {
    // Someone else's panel is open. Wait, close theirs, or show a hint.
    return;
}
```

More in [Panel management](panel-management).

## Letting the player press Escape

By default Escape is handled by the game. If your panel should own it:

```cpp
g_api->SetViewOwnsEscape(g_view, true);
```

```js
document.addEventListener('keydown', (event) => {
  if (event.key !== 'Escape') return;
  event.preventDefault();
  window.requestClose();     // your BindUIEvent listener
});
```

Only do this if the page really handles Escape. Otherwise you trap the player in a UI with no way out.

## Several views

One view per screen is usually enough; use your page's own routing for tabs. Use separate views when the surfaces are genuinely different — for example a persistent HUD plus an independent panel.

Draw order is controlled with:

```cpp
g_api->SetOrder(hudView, 0);
g_api->SetOrder(panelView, 10);   // drawn on top
```

## Health, and what to do with a broken view

`GetViewHealth(view)` tells you what the framework thinks:

| Value | Meaning |
| --- | --- |
| `kCreating` | Still loading. |
| `kDomReady` | DOM is ready. |
| `kLive` | Running normally. |
| `kLoadFailed` | The page failed to load. |
| `kDomReadyTimeout` | The page never became ready. |
| `kUnresponsive` | The page stopped responding. |
| `kJsError` | A JavaScript error left the view unhealthy. |

If a view is unhealthy, destroy it and create a fresh one. Calling into a dead handle does not fix anything:

```cpp
if (g_api->GetViewHealth(g_view) == PRISMA_UI_API::ViewHealth::kLoadFailed) {
    g_api->Destroy(g_view);
    g_view = 0;
    EnsureView();
}
```

The [view watchdog](view-watchdog) covers automatic recovery for focused views. The F12 inspector and `RegisterConsoleCallback` are how you find out *why* a page broke.

## Offscreen views are a different thing

`Hide()` is about on-screen presentation. V5 offscreen rendering (`SetViewOffscreen` and friends) gives you a texture for advanced consumers such as in-world screens. Treat offscreen mode as an advanced integration; do not use it as "hidden panel".

## Destroying

```cpp
if (g_api && g_api->IsValid(g_view)) {
    g_api->Unfocus(g_view);
    g_api->Hide(g_view);
    g_api->Destroy(g_view);
}
g_view = 0;
```

`Destroy` invalidates the handle and releases the page. Roles, input regions, escape ownership and callback registrations are cleared with it.

## Common mistakes

- **Creating the view before a save is loaded.** The handle exists but the UI cannot be shown properly. Create at `kPostLoadGame` / `kNewGame`.
- **Forgetting `Hide()` after creation.** The menu pops up on its own.
- **Calling `Show()` but not `Focus()`.** Visible panel, dead mouse.
- **Calling `Invoke` before `OnDomReady`.** Nothing runs; the page has no JavaScript context yet.
- **Never checking `IsValid()`.** After a reload or destroy, the old handle may be invalid.
- **Leaving an interactive view at `kUnspecified`.** Other mods cannot see it and open on top.

## Where to go next

- [Panel management](panel-management) — cooperating with other PrismaUI mods
- [The HTML side](html-views) — the page half
- [View watchdog](view-watchdog) — automatic recovery when a focused view dies
- [Troubleshooting](troubleshooting) — when the panel does not appear
