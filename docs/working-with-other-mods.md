---
title: Working with other PrismaUI mods
sidebar_label: With other mods
sidebar_position: 8
---

# Working with other PrismaUI mods

PrismaUI is a shared framework. A player may have several mods using it at once: your menu, someone's HUD, someone's settings page. These are the rules that keep the session playable when that happens.

## Declare what your view is

Set the role once, right after creating the view:

```cpp
g_api->SetViewRole(g_view, PRISMA_UI_API::ViewRole::kPanel);   // interactive screen
g_api->SetViewRole(g_hud,  PRISMA_UI_API::ViewRole::kWidget);  // passive HUD
```

- `kPanel` = a menu that should be respected by other mods.
- `kWidget` = always on screen, never blocks anything.
- `kUnspecified` = invisible to the cooperation checks. Do not leave a real menu here.

Without this, other mods cannot tell whether the screen is busy.

## Check before opening a panel

```cpp
if (g_api->IsAnyPanelVisible(g_view)) {
    // Another mod's panel is open.
    return;
}
```

That is the whole protocol. If every interactive mod does this, hotkeys stop stacking menus on top of each other. The argument is your own view, so you never block yourself.

## Do not steal focus

Focus belongs to one view at a time. Taking it while another panel is open is a hostile act unless the player asked for it:

- opening your panel from a hotkey: check first;
- opening from within the game's own menu systems: usually fine, you are the owner of that flow;
- opening from a quest or a scripted event: consider waiting for the player to close the current panel.

`GetFocusedView()` tells you who owns it; `HasAnyActiveFocus()` is the older yes/no.

## Do not touch other mods' views

`EnumerateViews` and `EnumerateViewsEx` are read-only surveys. Do not destroy, hide or re-role another mod's view. If you need it to go away, wait, or ask the player.

## HUD etiquette

- A HUD is a `kWidget`. It must not block menus from opening.
- Keep it small. A full-screen transparent page costs every frame.
- Hide the HUD while a menu is open if it would sit on top of the menu's content.
- Do not put a 60 fps animation in a HUD. Draw it once and update on change.

## Escape and input ownership

Only set `SetViewOwnsEscape(view, true)` when your page handles Escape and closes the panel. Otherwise you trap the player and break their habit of pressing Escape.

The same applies to controller buttons: bind only the buttons your panel needs, and let the rest pass through to the game.

## Output names are your namespace

Two rules keep mods from mixing up each other's files:

- **view folder**: `Data/PrismaUI_F4/views/<YourMod>/`. Use your plugin name; never a generic name like `ui` or `panel`.
- **translations**: `Data/Interface/Translations/<YourPlugin>_<lang>.json`. The name must match the name you pass to `RegisterTranslationsV4`.

If two mods use the same folder name, one overwrites the other and the failure looks like "my page is someone else's page".

## Console and logs

Both mods write to their own plugin logs; PrismaUI writes framework events to `PrismaUI_F4.log`. When reporting a conflict, that log plus your plugin log is what makes it diagnosable.

## Load order

PrismaUI itself is an F4SE plugin and is loaded by F4SE before game data. Your plugin decides when to ask for the API, so load order between Prisma mods does not matter as long as each one waits for `kGameDataReady`.

## Where to go next

- [Panel management](panel-management) — roles and focus in detail
- [A click-through HUD overlay](cookbook/hud-overlay) — the HUD pattern that plays well with menus
- [View lifecycle](view-lifecycle) — focus, hide and destroy order
