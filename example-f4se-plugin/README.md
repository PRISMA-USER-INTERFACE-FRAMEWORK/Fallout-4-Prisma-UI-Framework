# PrismaUI Example Plugin

This is the reference consumer project for **PrismaUI_F4 2.1.0**. It demonstrates how an F4SE plugin requests the Prisma API, creates an HTML/CSS/JS view, exchanges data with JavaScript, and handles input/lifecycle events.

## Supported desktop runtimes

PrismaUI_F4 2.1.0 supports:

- Fallout 4 `1.10.163` (OG)
- Fallout 4 `1.11.137+` (AE) when matching Address Library data is available

The framework deliberately rejects the intermediate `1.10.980-1.10.984` Next-Gen line. Do not publish a consumer as supporting that line merely because a CommonLib build can compile runtime slots for it.

Your HTML/CSS/JS assets are runtime-independent. The native plugin must still be built and tested for every Fallout runtime family you claim to support.

## Framework dependency

Install PrismaUI_F4 first. The production framework uses **Ultralight 1.4.0 in-process** with the D3D11 GPU-accelerated presentation path and a controlled CPU fallback.

Public download:

https://www.nexusmods.com/fallout4/mods/105454

Expected framework layout includes:

```text
Data/
├── F4SE/Plugins/
│   ├── PrismaUI_F4.dll
│   └── PrismaUI_F4.ini
└── PrismaUI_F4/
    ├── libs/
    ├── resources/
    ├── licenses/
    ├── runtime/
    └── views/
```

Do not install retired CEF/browser-host/subprocess components for the current framework.

## API header

`src/PrismaUI_F4_API.h` in this example is the canonical desktop SDK header and must stay byte-identical to the public repository copy. It contains `IVPrismaUI1` through `IVPrismaUI12` in one file. Repository CI verifies both copies against the pinned canonical Git blob.

For new code, request the lowest interface version you need. V10 provides panel coordination and selective input. V11 adds verified Fallout window-thread dispatch. V12 adds focused controller actions.

```cpp
#include "PrismaUI_F4_API.h"

auto* api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI10>();
if (!api) {
    return;
}
```

Request V11 or V12 only when your plugin actually uses their methods.

## File structure

```text
example-f4se-plugin/
├── src/
│   ├── PrismaUI_F4_API.h
│   ├── main.cpp
│   ├── keyhandler/
│   └── PCH.h
├── view/
├── build-and-deploy.bat
└── xmake.lua
```

## Build

1. Review the target/runtime configuration in `xmake.lua` for the Fallout family you intend to support.
2. Run `build-and-deploy.bat` or build with xmake directly.
3. Deploy the plugin DLL under `Data/F4SE/Plugins/`.
4. Deploy the example web assets under `Data/PrismaUI_F4/views/`.
5. Test on each runtime family you advertise.

Do not assume that one compiled binary automatically makes every historical Fallout runtime supported by PrismaUI itself.

## Starting your own plugin

You can copy this directory or use `prisma-mcp`'s `scaffold_plugin` tool.

After copying:

- rename the native target/plugin metadata;
- change the view folder/path to your own plugin-specific path;
- keep required JS/CSS/fonts/images local to your mod;
- null-check `RequestPluginAPI`;
- declare `ViewRole::kPanel` for interactive panels or `kWidget` for passive HUD views;
- use `IsAnyPanelVisible` before opening a panel when coordinating with other Prisma mods;
- capability-check V10 input regions before using `FocusOverlay` / `SetInputRegions`;
- use V11 for engine-affecting deferred callbacks when required;
- use V12 controller actions instead of a duplicate raw focused-view controller sink when appropriate.

## Networking

Do not rely on remote CDNs or Chromium-era browser networking assumptions. PrismaUI applies a restrictive Ultralight network policy. If your mod needs HTTP, WebSocket, authentication, or server push, implement it in native C++ and relay validated data to the view.

See the repository [Getting Started](../docs/getting-started.md), [API Reference](../docs/api-reference.md), [Controller Actions](../docs/controller-actions.md), and [Networking](../docs/networking.md) guides.
