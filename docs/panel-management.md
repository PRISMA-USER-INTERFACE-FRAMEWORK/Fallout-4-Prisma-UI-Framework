---
id: panel-management
title: Panel management
sidebar_label: Panel management
sidebar_position: 12
---

# Panel management

Fallout 4 sessions have more than one PrismaUI mod in them. This page is about not putting your menu on top of someone else's menu, and about letting your own panel be found by the others.

The calls here need `IVPrismaUI10`.

## The short version

```cpp
// Once, after creating the view:
g_api->SetViewRole(g_view, PRISMA_UI_API::ViewRole::kPanel);

// Before opening it:
if (g_api->IsAnyPanelVisible(g_view)) {
    return;   // someone else is open; do not stack
}

g_api->Show(g_view);
g_api->Focus(g_view, /*pauseGame=*/true);
```

That is the whole cooperation protocol. Everything below explains it.

## Why this exists

`IsAnyPanelVisible` answers one question: *is another Prisma-powered interface already open?*

Without it, two mods with a hotkey will both open when the player presses their keys, and the player ends up with two menus stacked on one screen, both trying to own input. With it, the second mod knows to wait.

It only works if every interactive mod declares itself. That is what roles are for.

## Roles

```cpp
enum class ViewRole : uint32_t {
    kUnspecified = 0,
    kWidget = 1,
    kPanel = 2,
};
```

| Role | Use it for | Counted as an open panel? |
| --- | --- | --- |
| `kUnspecified` | not decided yet | No |
| `kWidget` | a passive HUD or overlay | No |
| `kPanel` | inventory, settings, terminal, dialogue-like menu | Yes, while visible |

Declare the role right after you create the view:

```cpp
g_view = g_api->CreateView("MyPlugin/panel.html", OnDomReady);
if (g_view) {
    g_api->SetViewRole(g_view, PRISMA_UI_API::ViewRole::kPanel);
    g_api->Hide(g_view);
}
```

A view that takes input but stays at `kUnspecified` logs a one-time warning and stays invisible to the other mods' checks. That is the mistake that makes cooperation fail silently.

HUDs are widgets:

```cpp
g_api->SetViewRole(g_hud, PRISMA_UI_API::ViewRole::kWidget);
```

A widget never blocks a menu from opening, which is what you want for an always-on HUD.

## Opening only when the screen is free

```cpp
static void TryOpenPanel()
{
    if (!g_api || !g_api->IsValid(g_view)) return;

    if (g_api->IsAnyPanelVisible(g_view)) {
        // Another Prisma panel is open. Options:
        //  - do nothing (the player can close it and press your key again)
        //  - show your own "busy" hint
        //  - call Focus(g_view) to take focus, if that is what your mod should do
        return;
    }

    g_api->Show(g_view);
    g_api->Focus(g_view, /*pauseGame=*/true);
}
```

The argument is your own view: the call ignores the view you pass, so you never block yourself.

Pass `0` to ask about any panel in the session:

```cpp
const bool anyPanelOpen = g_api->IsAnyPanelVisible(0);
```

## Seeing who has focus

```cpp
PrismaView focused = g_api->GetFocusedView();
if (focused != 0 && focused != g_view) {
    REX::INFO("another Prisma view owns focus: {}", focused);
}
```

`HasAnyActiveFocus()` is the older yes/no version. Use these for diagnostics and for deciding whether to steal focus or wait.

## Closing in the right order

Always let go of input before hiding:

```cpp
g_api->Unfocus(g_view);
g_api->Hide(g_view);
```

Hiding a focused view without unfocusing can leave the game without input until the framework notices. Make the pair a habit.

## Selective input for overlays

A panel takes all mouse input while focused. Sometimes you want only a few rectangles to be interactive — a HUD where two buttons work and the rest of the screen still belongs to the game.

```cpp
if (!PRISMA_UI_API::HasPrismaCapability(
        PRISMA_UI_API::PrismaCapability::InputRegions)) {
    // This PrismaUI build has no input regions. Fall back to a normal panel.
    return;
}

const PRISMA_UI_API::InputRegion regions[] = {
    { 80, 80, 420, 240 },    // x, y, width, height in screen pixels
    { 560, 80, 180, 60 },
};

if (g_api->SetInputRegions(g_view, regions, 2)) {
    g_api->Show(g_view);
    g_api->FocusOverlay(g_view, /*pauseGame=*/false, /*disableFocusMenu=*/false);
}
```

Clear them when the overlay closes:

```cpp
g_api->SetInputRegions(g_view, nullptr, 0);
```

`IVPrismaUI10` existing does not prove input regions are enabled — check the capability, as above. There is a complete overlay in [A click-through HUD overlay](cookbook/hud-overlay).

## Escape

If your focused panel should close on Escape by itself:

```cpp
g_api->SetViewOwnsEscape(g_view, true);
```

```js
document.addEventListener('keydown', (event) => {
  if (event.key !== 'Escape') return;
  event.preventDefault();
  window.requestClose();
});
```

Only do this when the page has a working Escape handler. Otherwise the player is stuck.

## A complete cooperative panel

```cpp
static PRISMA_UI_API::IVPrismaUI10* g_api  = nullptr;
static PrismaView                   g_view = 0;

static void OnDomReady(PrismaView view)
{
    g_api->BindUIEvent(view, "requestClose", [](const char*) {
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
    });
}

static void CreatePanel()
{
    if (!g_api || g_view) return;

    g_view = g_api->CreateView("MyPlugin/panel.html", OnDomReady);
    if (!g_view) return;

    g_api->SetViewRole(g_view, PRISMA_UI_API::ViewRole::kPanel);
    g_api->Hide(g_view);
}

static void TryOpenPanel()
{
    if (!g_api || !g_api->IsValid(g_view)) return;
    if (g_api->IsAnyPanelVisible(g_view)) {
        REX::INFO("another Prisma panel is open; not opening");
        return;
    }

    g_api->Show(g_view);
    g_api->Focus(g_view, /*pauseGame=*/true);
}

static void ClosePanel()
{
    if (!g_api || !g_api->IsValid(g_view)) return;
    g_api->Unfocus(g_view);
    g_api->Hide(g_view);
}
```

## Common mistakes

- **Forgetting `SetViewRole`.** Your menu is real, but other mods do not see it.
- **Using `kPanel` for a HUD.** Every menu in the session will think the screen is busy.
- **Checking `IsAnyPanelVisible` after showing.** Check first, then show.
- **Hiding without unfocusing.** The game can keep waiting for input.
- **Assuming the V10 interface means input regions work.** Check `HasPrismaCapability(InputRegions)`.

## Where to go next

- [`SetViewRole`](api/SetViewRole) · [`IsAnyPanelVisible`](api/IsAnyPanelVisible) · [`GetFocusedView`](api/GetFocusedView)
- [A click-through HUD overlay](cookbook/hud-overlay) — input regions in practice
- [View lifecycle](view-lifecycle) — the states behind these calls
- [Working with other PrismaUI mods](working-with-other-mods) — the wider rules of the road
