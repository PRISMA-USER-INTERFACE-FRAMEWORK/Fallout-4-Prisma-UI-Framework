---
id: troubleshooting
title: Troubleshooting
sidebar_label: Troubleshooting
sidebar_position: 99
---

# Troubleshooting

Find your symptom, run the checks in order, stop at the first one that fails. Most PrismaUI problems are one of: a view that was never created, a view that is visible but not focused, or a page asset that did not load.

## First 30 seconds

1. Open **`Documents\My Games\Fallout4\F4SE\PrismaUI_F4.log`**. If it does not exist, the framework never ran — fix F4SE or the game version first.
2. Open your own plugin log. Look for your `CreateView` line.
3. If the view was created, press **F12** while it is focused and look at the Console.

That resolves most reports before you read further.

## Nothing appears on screen

1. Is `PrismaUI_F4.dll` in `Data\F4SE\Plugins\` and loaded by F4SE? The framework log proves it.
2. Is the Ultralight runtime present under `Data\PrismaUI_F4\`?
3. Did you call `RequestPluginAPI` at or after `kGameDataReady`? Earlier returns null.
4. Does the path in `CreateView` match the folder under `Data\PrismaUI_F4\views\` exactly, including capital letters?
5. Is the handle non-zero and `IsValid(view)` true?
6. Did you create it at `kPostLoadGame` / `kNewGame`? Creating before a world exists can give you a handle that has nothing to attach to.
7. Did you call `Hide()` after creation and never `Show()`? That is normal for menus — the open path must call `Show`.

## The panel is visible but clicks do nothing

A visible view is not a focused view.

```cpp
api->SetViewRole(view, PRISMA_UI_API::ViewRole::kPanel);
api->Show(view);
api->Focus(view, /*pauseGame=*/false);
```

Also check that another Prisma view is not holding focus (`GetFocusedView()`), and that your page has something focusable in the area the player clicks.

## A JavaScript listener does nothing

- Are the names identical? `window.requestClose` and `BindUIEvent(view, "requestClose", …)`.
- Was the listener registered in `OnDomReady`? Registration before the DOM exists does nothing useful.
- Did JavaScript throw earlier in the page? Register `RegisterConsoleCallback` and read the log.
- Is the page healthy? `GetViewHealth(view)`.

## The page is blank, white or unstyled

- Missing CSS link, or the CSS file is not in the view folder.
- No transparent background on `html, body` — a page with the default white background covers the game.
- A remote stylesheet or font was blocked — anything on a CDN is blocked. See [Networking](networking).
- The page threw before painting. Check the Console.

## The game crashes when the plugin calls PrismaUI

The usual cause is a null or mismatched API pointer:

```cpp
auto* api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI10>();
if (!api) {
    REX::CRITICAL("PrismaUI_F4 V10 unavailable");
    return;
}
```

Also check that the header you build against matches the installed framework, and that you are not calling a newer interface than the installed build provides.

## Translations do not appear

- File name must be `<pluginName>_<language>.json` under `Data\Interface\Translations\`, and `<pluginName>` must equal the string passed to `RegisterTranslationsV4`.
- Call registration in `OnDomReady`, and only then ask the page to render text.
- `RegisterTranslationsV4` returns `false` when the file is missing or malformed — check the return value.
- The page must use `window.PrismaL10N` only after registration; a module-level read runs too early.

## HUD suppression returns false or does nothing

`SuppressHUDWidget` needs a supported runtime with matching game support data. On an unsupported game version it fails closed instead of guessing. Check the return value and the framework log; if it fails, hide the widget through your own means or disable that part of your UI.

## Activate-choice filtering behaves differently than old guides say

The released behaviour is narrower than some older documentation:

- `EnableActivateChoiceFilter(true, dropDefaultTake)` enables capture; `dropDefaultTake` is ignored.
- `SuppressActivateChoicePerk` is a compatibility placeholder and does not remove perk rows.
- Use `GetActivateChoiceLabel` and `TriggerActivateChoice` for the supported read/trigger path.

## Input leaks through to the game

- For a normal interactive panel, use `Focus`, not just `Show`.
- For an overlay, use V10 input regions and keep the rectangles in sync with the visible controls. A wrong rectangle lets clicks through or eats clicks over empty space.
- For Escape, only take ownership (`SetViewOwnsEscape`) when the page actually handles Escape.

## 3D and on-mesh views

- Wait until the view has rendered before expecting a texture.
- Set the offscreen size to the target surface's aspect ratio.
- Bind after the target geometry exists; rebind if the game rebuilds it.
- Unbind while the original geometry is still valid.
- Never release the framework-owned texture yourself.

Mesh binding is an advanced, debug-gated integration. It is off by default, so it is not the right tool for a normal overlay.

## The installation has leftover old files

If the game folder has a mix of old and current runtime files, reinstall the PrismaUI package cleanly. Do not hand-merge runtime generations.

Do not delete the current Ultralight libraries: `AppCore.dll`, `Ultralight.dll`, `UltralightCore.dll` and `WebCore.dll` are part of the shipping runtime.

## Reporting a bug

Include:

- PrismaUI version and where it came from;
- your exact Fallout 4 version;
- F4SE version;
- `Documents\My Games\Fallout4\F4SE\PrismaUI_F4.log`;
- your plugin log;
- whether the view is a normal screen view, an offscreen view or a mesh-bound view;
- what you did just before it happened, and whether it happens again after reopening the panel.

## Where to go next

- [Debugging your view](debugging) — the tooling behind these checks
- [Limitations](limitations) — when the answer is "not supported"
- [FAQ](faq) — quick questions
- [Your first panel](quick-start) — a known-good setup to compare against
