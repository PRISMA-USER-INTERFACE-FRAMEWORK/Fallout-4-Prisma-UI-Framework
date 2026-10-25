---
id: vanilla-ui-suppression
title: Vanilla UI suppression
sidebar_label: Vanilla UI suppression
sidebar_position: 8
---

# Vanilla UI suppression

PrismaUI can hide or block selected pieces of Fallout's own UI while your replacement is open — a vanilla menu, a HUD widget, or the activate-choice button strip. These calls work on Fallout's UI objects; they have nothing to do with the web view itself.

Use them when you are replacing part of the vanilla interface, not when you are adding a new one.

## Suppress a whole menu

```cpp
g_api->SuppressVanillaMenu("PipboyMenu", true);

// later, when your replacement closes
g_api->SuppressVanillaMenu("PipboyMenu", false);
```

- The name is the vanilla `MENU_NAME` (`PipboyMenu`, `ContainerMenu`, `WorkshopMenu`, …).
- Suppression is reapplied whenever that menu is opened again, for as long as your suppression state is on.
- There is no `IsMenuSuppressed` query. Track your own state if you need to know whether *you* asked for it.

To close a menu that is open right now, without blocking future opens:

```cpp
g_api->CloseVanillaMenu("ContainerMenu");
```

To close and keep it closed, call both:

```cpp
g_api->SuppressVanillaMenu("ContainerMenu", true);
g_api->CloseVanillaMenu("ContainerMenu");
```

## Suppress it only sometimes

A predicate is evaluated each time the menu tries to open:

```cpp
static bool ShouldSuppressContainer()
{
    return g_replacementActive;
}

g_api->SuppressVanillaMenuIf("ContainerMenu", &ShouldSuppressContainer);
```

Pass `nullptr` to remove the predicate. It runs synchronously on the game thread, so keep it short and non-blocking — it is not a place for file I/O or waits.

## Suppress a HUD widget

```cpp
if (!g_api->SuppressHUDWidget("HUDCompass", true)) {
    REX::WARN("HUDCompass suppression unavailable on this runtime");
}
```

- Unknown class names return `false`.
- The HUD path needs supported runtime data; when it cannot resolve safely, the call fails closed rather than patching a guessed address.
- Supported: Old-Gen 1.10.163 and Anniversary Edition 1.11.137+ with the matching runtime data.

Always check the return value. A "suppression" that silently did nothing is worse than an honest failure.

## The replacement pattern

Suppress, close, show yours; reverse it when closing:

```cpp
static PRISMA_UI_API::IVPrismaUI10* g_api  = nullptr;
static PrismaView                   g_panel = 0;

void OpenReplacement()
{
    if (!g_api || !g_api->IsValid(g_panel)) return;
    if (g_api->IsAnyPanelVisible(g_panel)) return;   // another mod's panel is open

    g_api->SuppressVanillaMenu("ContainerMenu", true);
    g_api->CloseVanillaMenu("ContainerMenu");

    g_api->SetViewRole(g_panel, PRISMA_UI_API::ViewRole::kPanel);
    g_api->Show(g_panel);
    g_api->Focus(g_panel, /*pauseGame=*/true);
}

void CloseReplacement()
{
    if (!g_api || !g_api->IsValid(g_panel)) return;

    g_api->Unfocus(g_panel);
    g_api->Hide(g_panel);
    g_api->SuppressVanillaMenu("ContainerMenu", false);
}
```

Track whether *your* feature requested suppression, and undo it when the feature is disabled or your plugin unloads. Leaving a vanilla menu suppressed after your UI is gone reads as a bug to the player.

## Activate-choice APIs

The V7/V8 activate-choice calls are narrower than older guides describe:

```cpp
g_api->EnableActivateChoiceFilter(true, false);           // enables capture
g_api->GetActivateChoiceLabel(buttonIndex, buffer, size); // read a captured label
g_api->TriggerActivateChoice(buttonIndex);                // trigger a captured choice
```

- `EnableActivateChoiceFilter` enables the capture path. Its second argument (`dropDefaultTake`) is kept for compatibility and ignored.
- `SuppressActivateChoicePerk` is a compatibility placeholder: it does not remove perk rows.
- `GetActivateChoiceLabel` / `TriggerActivateChoice` are the supported read/trigger path when capture is available.

Do not build a public feature around `SuppressActivateChoicePerk` or the ignored argument.

## Menu events still happen

Closing or suppressing a vanilla menu does not erase the game's own `MenuOpenCloseEvent` traffic. If your plugin also listens to menu events, guard against reacting to changes your own code just caused:

```cpp
// pseudo-code shape: track the reason for the close
if (g_closingForReplacement) {
    g_closingForReplacement = false;
    return;
}
```

## Runtime notes

- Old-Gen **1.10.163** and Anniversary Edition **1.11.137+** with matching runtime data are supported.
- The 1.10.980–1.10.984 line is rejected by the current framework.
- Engine-hook-backed helpers should be tested on each runtime family you claim. A method existing in the header does not promise every engine feature works everywhere.

## Where to go next

- [`SuppressHUDWidget`](api/SuppressHUDWidget) · [`SuppressVanillaMenu`](api/SuppressVanillaMenu) · [`CloseVanillaMenu`](api/CloseVanillaMenu) · [`SuppressVanillaMenuIf`](api/SuppressVanillaMenuIf)
- [Activate choice pages](api/GetActivateChoiceLabel) — capture and trigger
- [Panel management](panel-management) — do not stack your UI on another mod's
- [Limitations](limitations) — runtime-specific behaviour
