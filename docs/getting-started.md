---
title: Install PrismaUI
sidebar_label: Install PrismaUI
sidebar_position: 3
---

# Install PrismaUI

This page covers two things: getting PrismaUI working as a player, and setting up your machine to build mods that use it.

## If you are a player

You need this only if a mod page says it requires PrismaUI.

1. **Install the framework.** Download the PrismaUI release and install it with your mod manager (the FOMOD installer walks through it), or copy the archive contents into `Data\` by hand. The important files are:
   - `Data\F4SE\Plugins\PrismaUI_F4.dll`
   - `Data\PrismaUI_F4\` (the Ultralight runtime and the boot assets)
2. **Install F4SE** for your game version, if you have not already.
3. **Start the game once** and quit when you reach the main menu or load a save.
4. **Check the log.** Open `Documents\My Games\Fallout4\F4SE\PrismaUI_F4.log`. It should exist and list the runtime it loaded. If it does not exist, the DLL never ran — check that F4SE is installed for the exact game version you are running.

That is it. Mod pages that require PrismaUI will now find it.

**Version rule:** install the version of PrismaUI that matches your game version. Old-Gen 1.10.163 and Anniversary Edition 1.11.137+ are supported. The 1.10.980–1.10.984 line is intentionally not supported, and PrismaUI will refuse to load rather than half-work.

**VR:** Fallout 4 VR uses a separate PrismaUI package. Do not mix the flat and VR files.

If a UI is blank, see [Troubleshooting](troubleshooting).

## If you are building a mod

### What you need

- **A plugin project that already builds.** PrismaUI plugs into a normal F4SE C++ plugin. The [example plugin](https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Fallout-4-Prisma-UI-Framework/tree/main/example-f4se-plugin) shows the full setup, including the key handler used in [Your first panel](quick-start).
- **Visual Studio 2022** (Desktop development with C++) or the same compiler setup your plugin already uses.
- **PrismaUI installed in the game folder**, so you can test what you build.
- **One of the two API headers** from `src/` in this repository:
  - `PrismaUI_F4_Modern_API.h` — preferred for new code. Feature tables you request individually.
  - `PrismaUI_F4_API.h` — the numbered `IVPrismaUI1`–`IVPrismaUI12` interfaces. Use it when maintaining an existing plugin.

Copy the header you choose into your project. Do not copy the framework DLL; it lives in the game folder.

### Where your files go

Everything the game needs at runtime sits under `Data`:

```text
Data/
├── F4SE/Plugins/
│   └── MyPlugin.dll
├── PrismaUI_F4/
│   ├── PrismaUI_F4.dll          (installed with the framework)
│   ├── ultralight/              (installed with the framework)
│   └── views/
│       └── MyPlugin/
│           ├── index.html       (your page)
│           ├── app.css
│           └── app.js
└── Interface/Translations/      (only if you localize, see Translations)
    └── MyPlugin_en.json
```

`CreateView("MyPlugin/index.html", ...)` resolves to `Data/PrismaUI_F4/views/MyPlugin/index.html`. If you use Mod Organizer 2, keep this same layout inside one mod folder — the paths are virtual paths under `Data`.

Keep every file your page needs in that folder. No CDNs, no external fonts, no remote scripts.

### Quick check before you write code

1. Game, F4SE and PrismaUI are installed.
2. You have started the game once and `Documents\My Games\Fallout4\F4SE\PrismaUI_F4.log` exists.
3. Put a single `index.html` containing `<h1>Hello</h1>` in `Data\PrismaUI_F4\views\MyPlugin\`.
4. Build a plugin that calls `CreateView("MyPlugin/index.html", nullptr)` at `kPostLoadGame` and `Show` it.
5. Press whatever opens it. If you see the heading, your setup is correct and you can move on.

### A minimal "is PrismaUI there?" plugin

```cpp
#include "PrismaUI_F4_API.h"

static PRISMA_UI_API::IVPrismaUI10* g_api = nullptr;

static void OnMessage(F4SE::MessagingInterface::Message* message)
{
    switch (message->type) {
    case F4SE::MessagingInterface::kGameDataReady:
        g_api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI10>();
        if (!g_api) {
            REX::CRITICAL("PrismaUI_F4 is not installed");
        } else {
            REX::INFO("PrismaUI_F4 is ready");
        }
        break;

    case F4SE::MessagingInterface::kPostLoadGame:
        if (g_api) {
            PrismaView view = g_api->CreateView("MyPlugin/index.html", nullptr);
            if (view) {
                g_api->Show(view);
            }
        }
        break;

    default:
        break;
    }
}
```

If that shows your page, the rest of the documentation is going to work exactly as written.

## Installing the build

The DLL goes in `Data\F4SE\Plugins\`, the page goes in `Data\PrismaUI_F4\views\<YourPlugin>\`. Nothing else is needed.

## Where to go next

- [Your first panel](quick-start) — build a complete opening/closing panel.
- [The HTML side](html-views) — what your page can do and how it talks to the plugin.
- [Cookbook](/docs/cookbook/) — complete examples for HUDs, previews, controllers and translations.
- [Troubleshooting](troubleshooting) — when the window is blank, the log is empty or nothing appears.
