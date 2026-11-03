---
title: FAQ
sidebar_label: FAQ
sidebar_position: 21
---

# FAQ

## Do I need C++ to use PrismaUI?

For a full mod, usually yes: a native plugin creates the view, controls it and bridges to the game. There is also a Papyrus bridge that can create and show a view from a quest script — enough for simple panels. See [Open a panel from Papyrus](cookbook/from-papyrus).

## Which Fallout 4 versions work?

Old-Gen 1.10.163 and Anniversary Edition 1.11.137 and newer, when the matching runtime data is installed. The 1.10.980–1.10.984 Next-Gen line is intentionally not supported. Fallout 4 VR uses a separate package.

## I installed it and nothing appears. What do I check first?

1. Does `Documents\My Games\Fallout4\F4SE\PrismaUI_F4.log` exist? If not, the framework DLL never ran — F4SE or game version problem.
2. Does the log show your view being created? If not, your plugin never called `CreateView`, or it asked before `kGameDataReady`.
3. Is the view shown **and focused**? `Show` draws it; `Focus` gives it input.
4. Open F12 (inspector) and look for page errors.

Full list in [Troubleshooting](troubleshooting).

## Can I use React, Vue or Svelte?

Yes, if you build it into static files and load them locally. Bundle the output into your view folder; there is no build tool inside the game. Plain HTML/CSS/JS is fine too, and easier to debug.

## Can the page call the internet?

No. Remote fetches, WebSockets, Workers and EventSource are blocked on purpose. Do networking in C++ and pass results to the page through `InteropCall`. See [Networking](networking).

## Why is my panel a white box?

Almost always CSS: the page has no transparent background, or a stylesheet failed to load. Start with:

```css
html, body { margin: 0; width: 100%; height: 100%; background: transparent; }
```

## Can I make a HUD that keeps the game running?

Yes. Use an overlay-focused view with input regions so only your buttons take clicks. Complete example: [A click-through HUD overlay](cookbook/hud-overlay).

## The player can't click my buttons. Why?

The view is probably visible but not focused, or something else owns focus. Check `GetFocusedView()` and call `Focus(view, pauseGame)`.

## How do I make the panel close with Escape?

Bind `SetViewOwnsEscape(view, true)` and handle Escape in the page, then hide/unfocus from the callback. Only do this when your page really handles it.

## How do I add translations?

Ship `Data/Interface/Translations/<Plugin>_<lang>.json`, call `RegisterTranslationsV4(view, "<Plugin>")` in `OnDomReady`, and use `window.PrismaL10N.t()` in the page. Recipe: [Translate your interface](cookbook/translate-your-ui).

## Does it work in VR?

A separate VR provider and package exist. Some flat-only features (game-thread dispatch, for example) are not available there. Check [Limitations](limitations).

## How do I show a 3D item?

Use the ModelPreview bridge (`window.__prismaUI_modelPreview`) from the page. Complete example: [Show a 3D item preview](cookbook/item-preview).

## Why does the game stutter while my HUD is open?

The page renders into the game's frame. A heavy page costs frames. Reduce repaints, stop animations when nothing changes, and do not run work every frame. See [The HTML side](html-views).

## Can two PrismaUI mods run together?

Yes, that is expected. The rules are: declare a role (`kPanel` / `kWidget`), check `IsAnyPanelVisible` before opening a menu, and never touch another mod's view. See [Working with other PrismaUI mods](working-with-other-mods).

## Where do I report a bug?

Include your `PrismaUI_F4.log`, the mod that owns the view, the exact game version, and what you did just before it happened. That log answers most of the questions up front.

## Where to go next

- [What is PrismaUI F4?](what-is-prismaui)
- [Your first panel](quick-start)
- [Troubleshooting](troubleshooting)
- [Limitations](limitations)
