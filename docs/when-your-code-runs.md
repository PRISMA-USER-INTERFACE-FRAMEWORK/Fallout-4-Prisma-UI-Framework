---
title: When your code runs
sidebar_label: When your code runs
sidebar_position: 5
---

# When your code runs

Most PrismaUI bugs are timing bugs: the plugin asks for the API too early, creates a view before a world exists, or sends data before the page has a DOM. This page is the order things happen in, and where each kind of work belongs.

## The order

```
game starts
  │
  ├─ F4SE loads plugins                 → your F4SEPlugin_Load runs
  │
  ├─ kGameDataReady                     → PrismaUI is loaded; ask for the API here
  │
  ├─ main menu
  │
  ├─ player loads a save / starts new   → kPostLoadGame / kNewGame
  │                                        create and show views here
  │
  ├─ gameplay
  │    ├─ your keys, timers, game events → toggle / update / send data
  │    └─ page callbacks                 → already on the game thread
  │
  ├─ player quits / reloads a world     → views may die; check IsValid
  │
  └─ game exits                         → tearing down views here is rarely needed
```

## kGameDataReady — the API becomes available

This is the first message where `RequestPluginAPI` works:

```cpp
case F4SE::MessagingInterface::kGameDataReady:
    g_api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI10>();
    if (!g_api) {
        REX::CRITICAL("PrismaUI_F4 is not installed");
        return;
    }
    break;
```

If you call `RequestPluginAPI` in `F4SEPlugin_Load`, it returns null even with PrismaUI installed. That is the most common "PrismaUI not found" false alarm.

Do **not** create views here. The game has no loaded world yet, and a panel created this early has nothing to attach to.

## kPostLoadGame and kNewGame — create views

```cpp
case F4SE::MessagingInterface::kPostLoadGame:
case F4SE::MessagingInterface::kNewGame:
    CreateView();
    break;
```

Both messages mean "a world now exists". `kPostLoadGame` fires after a save loads; `kNewGame` fires once a new game's first world is ready. Treat them the same for view creation.

If the player loads another save later, these fire again. Recreate the view if the old handle is dead:

```cpp
static void CreateView()
{
    if (!g_api) return;
    if (g_view && g_api->IsValid(g_view)) return;

    g_view = g_api->CreateView("MyPlugin/panel.html", OnDomReady);
    if (g_view) {
        g_api->SetViewRole(g_view, PRISMA_UI_API::ViewRole::kPanel);
        g_api->Hide(g_view);
    }
}
```

## OnDomReady — the page exists

`OnDomReady` runs when the page has a DOM and JavaScript can be called. Everything page-facing belongs here:

- event bindings (`BindUIEvent`, `RegisterJSListener`);
- controller actions (`BindControllerAction`);
- translations (`RegisterTranslationsV4`);
- initial data (`InteropCall`, `Invoke`);
- console capture (`RegisterConsoleCallback`).

If the page is reloaded, `OnDomReady` runs again for the new document. Re-register what the page needs.

## Gameplay — events, keys and timers

Once your view exists, anything can trigger it:

- **A key or controller button** in your plugin's input handler. Run your own handler on the game thread.
- **A game event** (a quest stage, a menu opening, an item being added). F4SE event sinks run on the game thread.
- **A timer or background thread.** You own that thread, so post the work back to the game thread first (`DispatchToGameThread`, or your own existing game-thread queue).
- **The page itself.** `BindUIEvent` / `RegisterJSListener` / `BindGameThreadUIEvent` callbacks are already on the game thread.

## Page callbacks are already safe

Inside a PrismaUI callback you can touch `RE::` directly:

```cpp
g_api->BindUIEvent(view, "dropItem", [](const char* json) {
    auto* player = RE::PlayerCharacter::GetSingleton();   // safe here
    // ...
});
```

You do not need to wrap these in another `AddTask`. Doing so is harmless but adds a frame of latency.

The cache: the guarantee is for callbacks PrismaUI delivers. Your own threads are still your responsibility.

## When a view can die

Check `IsValid()` before using a stored handle after:

- loading a save or starting a new game;
- a world transition (entering/leaving an interior is usually fine, but reloads are not);
- the player closing or destroying the view;
- recoverable view failures (see `GetViewHealth`).

A cheap pattern is `EnsureView()`: if the handle is invalid, create a new one. Call it before opening the panel rather than sprinkling checks everywhere.

## VR has its own provider

The VR provider is a separate DLL and advertises a different feature set. Everything on this page still applies, but some features (game-thread dispatch, for example) are flat-only. Check the [Limitations](limitations) page before assuming a call exists on VR.

## Where to go next

- [Your first panel](quick-start) — a working example of this order
- [View lifecycle](view-lifecycle) — what the view states mean
- [Debugging your view](debugging) — when something in the sequence goes wrong
- [Packaging your mod](packaging) — shipping the result
