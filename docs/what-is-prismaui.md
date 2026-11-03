---
id: what-is-prismaui
title: What is PrismaUI F4?
sidebar_label: What is PrismaUI F4?
sidebar_position: 1
---

# What is PrismaUI F4?

PrismaUI F4 lets you build Fallout 4 interfaces with HTML, CSS and JavaScript instead of Scaleform SWFs.

If you can make a web page, you can make a menu, a HUD, a terminal screen or a Pip-Boy panel in Fallout 4. You write the page like any other web page, then a small F4SE plugin (or a Papyrus script) tells PrismaUI when to show it and what to put in it.

```
your plugin  ──create / show / send data──▶  PrismaUI_F4.dll
                                                  │
                                                  │  Ultralight 1.4.0, in-process
                                                  ▼
                                            Fallout 4 frame
your HTML/CSS/JS  ──clicks, keys, data──▶  your plugin
```

## Who it is for

- **Players** who downloaded a mod that needs PrismaUI. Follow [Install PrismaUI](getting-started).
- **Mod authors** who want a real UI without fighting Scaleform. Follow [Install PrismaUI](getting-started), then [Your first panel](quick-start). It takes about ten minutes and builds a panel you can open in game.
- **Experienced plugin authors** who just want the calls. Open the [API reference](api-reference) or jump into the [Cookbook](/docs/cookbook/) for a complete example of the thing you are trying to do.

## What you can build

- **Menus and settings pages** with normal buttons, sliders, text fields and tabs.
- **HUD widgets and overlays** that sit over the game with click-through areas you define.
- **In-game screens and terminals** by binding a view to a screen in the world.
- **Item and model previews** with real 3D meshes rendered by the game, not fake images.
- **Controller-first interfaces** with your own button prompts and focus.
- **Localized interfaces** that follow the player's game language.

## How it works

1. **You write a web page.** Put `index.html`, CSS, JavaScript, fonts and images in `Data/PrismaUI_F4/views/YourPlugin/`.
2. **Your plugin creates the view.** `CreateView("YourPlugin/index.html", OnDomReady)` tells PrismaUI to load the page and hand you a `PrismaView` handle.
3. **You control the view.** Show, hide, focus, move data in, listen for clicks — all through the PrismaUI API.
4. **PrismaUI owns the rest.** Ultralight, rendering, input routing, focus and D3D11 composition all run inside the Fallout process.

There is no separate browser process and no host program to ship. `PrismaUI_F4.dll` plus the Ultralight runtime is the whole framework.

## Supported game versions

- Fallout 4 **1.10.163** (Old-Gen)
- Fallout 4 **1.11.137 and newer** (Anniversary Edition family) when the matching runtime data is installed
- Fallout 4 **VR** uses its own package and its own provider DLL

The 1.10.980–1.10.984 Next-Gen line is deliberately not supported. If the framework refuses to load there, that is working as intended. These boundaries match the framework README (source: README.md).

## The two ways to call it

New code should use **`PrismaUI_F4_Modern_API.h`**. It exposes small feature tables (View, Controller, Interop, GameThread, Localization, Render, Input, Menu) and you only ask for the ones you need:

```cpp
#include "PrismaUI_F4_Modern_API.h"
using namespace PRISMA_UI_FLAT_API;

ViewAPI viewApi{};
if (!Discover<ApiFeature::View>(ViewApiVersion, viewApi)) {
    // PrismaUI is missing or too old for this table.
    return;
}
```

Existing plugins built against the numbered `PrismaUI_F4_API.h` header keep working unchanged. `IVPrismaUI1` through `IVPrismaUI12` are still there, and [Your first panel](quick-start) uses them because it is the shortest complete example:

```cpp
auto* api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI10>();
if (!api) {
    // PrismaUI is not installed or is too old.
    return;
}
```

Both paths control the same framework. Pick one and stay with it.

## What PrismaUI is not

- **Not a Scaleform editor.** It draws its own layer and can optionally hide parts of the vanilla UI while your interface is open.
- **Not a browser.** It runs Ultralight, not Chrome. Web Workers, WebSockets and remote fetches from the page are blocked on purpose. Do network work in C++ and pass the result to the page.
- **Not a game-logic API.** Reading forms, editing the world and Papyrus calls stay in your plugin. PrismaUI handles the UI layer and the bridge between the page and your plugin.
- **Not a CDN client.** Bundle your fonts, scripts and images with the mod. Do not depend on remote assets.

## Where to start

1. [Install PrismaUI](getting-started) and check the log line.
2. Build [Your first panel](quick-start).
3. Pick a recipe from the [Cookbook](/docs/cookbook/) that looks like what you want to make.
4. Use the [API reference](api-reference) when you need the exact call.
5. Stuck? Read [Troubleshooting](troubleshooting) or the [Limitations](limitations).
