---
title: Packaging your mod
sidebar_label: Packaging your mod
sidebar_position: 6
---

# Packaging your mod

This is how to ship a PrismaUI mod so it installs cleanly — and so it still works when the player uses a mod manager.

## The install layout

Everything lives under `Data`:

```text
Data/
├── F4SE/
│   └── Plugins/
│       └── MyPlugin.dll
├── PrismaUI_F4/
│   └── views/
│       └── MyPlugin/
│           ├── index.html
│           ├── app.css
│           ├── app.js
│           ├── fonts/
│           └── images/
├── Interface/
│   └── Translations/
│       ├── MyPlugin_en.json
│       └── MyPlugin_de.json
└── MyPlugin.esp                (only if you have one)
```

Rules that matter:

- **`MyPlugin/` is your namespace.** Use a folder name that is yours (the plugin name is a good choice). Two mods with a `<name>` folder of the same name overwrite each other.
- **The view path in `CreateView` is relative to `Data/PrismaUI_F4/views/`.** `CreateView("MyPlugin/index.html", ...)` maps to the folder above.
- **Translations are keyed by plugin name**, not by folder name. `MyPlugin_en.json` matches `RegisterTranslationsV4(view, "MyPlugin")`.
- **Keep the case exactly the same.** Windows is forgiving; some tools and archives are not.

## Packing into a BA2

Shipping loose files is fine and simplest. Packing into a BA2 is nicer for large mods and works with the same view paths.

When you pack, keep the folder structure relative to `Data`:

```text
PrismaUI_F4/views/MyPlugin/index.html
PrismaUI_F4/views/MyPlugin/app.css
Interface/Translations/MyPlugin_en.json
```

Use the game's `Archive2` tool (shipped with the Creation Kit) or your mod manager's packer. Set the archive to include the files without the `Data/` prefix — the fallback folder in the packer is what adds it.

If you are unsure, start with loose files; a working loose-file mod is worth more than a broken BA2.

## Mod managers

Mod Organizer 2 installs a mod into a virtual `Data` folder. Keep the same tree inside your archive root:

```text
MyPlugin-1.0.0/
└── (the Data contents described above)
```

The same archive works for MO2, Vortex and a manual install. Do not include a top-level `Data/` folder inside the archive — the contents of `Data` are the archive root.

## FOMOD installer

A FOMOD is optional. If you make one, useful options are:

- install the plugin and views (always);
- install optional extra languages;
- install the developer headers for people building against your mod;
- install optional test pages.

An FOMOD mainly prevents players from installing a test page by accident. It does not change the file layout.

## Version and update safety

- **Never rename the view folder between versions** unless you also change the plugin. The old folder is what the plugin asks for.
- **Keep translation file names stable.** Renaming `<plugin>_en.json` silently disables translations.
- **Do not ship the PrismaUI framework DLL.** It is installed separately and updated by the player. Ship only your plugin and your view.

## Before you publish

Test all of these once:

1. The mod installs into a clean game with F4SE and PrismaUI already present.
2. The panel opens with your key or event.
3. `Documents\My Games\Fallout4\F4SE\PrismaUI_F4.log` shows no errors from your view.
4. The page loads with the DevTools (F12) closed — no external fonts, scripts or stylesheets.
5. The mod works in a second play session (load a save, quit, restart, load again).
6. The folder name of your view does not collide with another installed PrismaUI mod.
7. If you ship translations: switch the game language and check the strings change.

## Where to go next

- [Debugging your view](debugging) — what to check before publishing
- [Translations](translations) — the translation file contract
- [The HTML side](html-views) — asset and networking rules
- [Installing PrismaUI](getting-started) — what the player needs
