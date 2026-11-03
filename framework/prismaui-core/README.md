# PrismaUI_F4

**PrismaUI_F4** is an F4SE plugin that embeds **Ultralight 1.4.0** into Fallout 4. It lets mod authors create interactive
HTML/CSS/JavaScript overlays - menus, HUDs, settings panels, terminals - without touching
Scaleform or the game's native UI system. The retired CEF host, subprocess, and runtime tree are
not part of production packages.

## How It Works

`PrismaUI_F4.dll` owns the Ultralight views, input routing, and frame composition. Consumer plugins
continue to create views through the existing PrismaUI API and install their HTML assets under
`Data/PrismaUI_F4/views/`.

## Architecture

```
Fallout 4 process
├── F4SE
│   └── PrismaUI_F4.dll
│       ├── Ultralight views
│       ├── Frame compositor
│       ├── FocusMenu
│       └── Public API
│
└── YourPlugin.dll                   ← your F4SE plugin
    ├── PrismaUI_F4_API.h            ← frozen V1-V12 compatibility SDK
    ├── PrismaUI_F4_Modern_API.h     ← flat Core/Controller/GameThread/Callbacks SDK tables
    └── main.cpp                     ← request API, create views, wire hotkeys
```

## Quick Start

1. For new consumers, copy `PrismaUI_F4_Modern_API.h` into your plugin's `src/` folder. Discover
   each feature table independently on `kGameDataReady`:
   ```cpp
   PRISMA_UI_FLAT_API::CoreAPI core{};
   PRISMA_UI_FLAT_API::GetAPI(
       PRISMA_UI_FLAT_API::ApiFeature::Core,
       PRISMA_UI_FLAT_API::CoreApiVersion,
       &core,
       sizeof(core));
   ```
2. Create a view (starts visible - hide it immediately):
   ```cpp
   PrismaView view = core.CreateView("mymenu.html", OnDomReady);
   core.Hide(view);
   ```
3. Place your HTML file at `Data/PrismaUI_F4/views/mymenu.html`.
4. Show, focus, hide from a key handler.

Existing plugins may continue copying `PrismaUI_F4_API.h` and using
`RequestPluginAPI`; that virtual ABI is frozen and remains supported. Legacy V1-V12 remain
supported on flat Fallout 4, and V9-V12 intentionally use the frozen epoch tokens `0x88` through
`0x8B`. No migration is required for consumers using the canonical legacy header; private extension
headers must be refreshed from their owning API contract.

The flat `Callbacks` feature (`PRISMA_UI_FLAT_API::ApiFeature::Callbacks`, version 1) adds userdata
variants of `CreateViewEx`, `InvokeEx` and `RegisterConsoleCallbackEx`, which hand a caller `void*`
back to the callback unchanged.

See `docs/` for the full walkthrough, complete API reference, HTML/JS integration guide, view
lifecycle rules, working examples, and current backend limitations.

Focused controller actions beyond D-pad/A/B are documented in [`docs/CONTROLLER_ACTIONS.md`](docs/CONTROLLER_ACTIONS.md).

## File Layout (installed)

```
Data/
├── F4SE/Plugins/
│   ├── PrismaUI_F4.dll
│   └── PrismaUI_F4.ini
└── PrismaUI_F4/
    ├── libs/                      ← Ultralight DLLs
    ├── resources/                 ← Ultralight data files
    ├── licenses/                  ← PrismaUI and Ultralight legal files
    ├── runtime/                   ← pinned runtime manifest
    └── views/                     ← all HTML files from all consumer plugins land here
        ├── inventory.html   (from PrismaInventory_F4)
        └── mymenu.html      (from your plugin)
```

MO2 merges every mod's `PrismaUI_F4/views/` folder into one virtual directory. View filenames must be unique across all mods.

## Building the Framework

PrismaUI_F4 uses **xmake** and one flat DLL. The build retains CommonLibF4's three runtime slots
for its ABI, but Prisma Matrix accepts Old-Gen 1.10.163, the Anniversary Edition family from
1.11.137 onward, and Next-Gen 1.10.984 (concept-level, static RE, TODO-verify live; see
`src/RuntimeSupportPolicy.h:IsSupportedGameVersion`) when matching F4SE support and its Address
Library database are available.
This concept-level version eligibility contract intentionally includes later `1.x` minor versions
such as `1.12.0`; compatibility with future executables remains unverified. NG 1.10.984 supported (static RE; not yet live-tested). NG versions other than 1.10.984 are unsupported. AE local-map / Interface3D are wired but not live-tested.

Native menu-controller conversion is a separate flat Fallout adapter. The current RE record
contains hash-bound static converter, sender, input-field, and downstream consumer evidence for OG
`1.10.163` and AE `1.11.240` only. Live behavior is still untested. Other
eligible AE executables still load PrismaUI, but fail closed for the native converter path.
`1.11.137+` is not a controller-adapter guarantee; 1.11.191 and 1.11.221 are intentionally
outside this adapter's supported scope.
FO4VR controller/runtime support is deferred to the separate VR plugin work tracked under #740, #741, and #742.

The shipping build links the pinned Ultralight SDK and stages its required DLLs, resources, and
legal files. `PrismaUI_F4VR.dll` remains a separate compile target.

See [`BUILD.md`](BUILD.md) for the current web-assets, Ultralight, flat, VR, validation, and release packaging
commands. GitHub PR CI runs the same static/unit gate and native clang-cl/xwin shipping compile used to
validate changes before merge.

## Building the Example Plugin

We provide a complete, working reference plugin in the [example-f4se-plugin/](example-f4se-plugin) directory that demonstrates standard integrations (bridges, console logging, view creation).

To build and deploy the example plugin:
1. Navigate to the `example-f4se-plugin/` directory.
2. Double-click the `build-and-deploy.bat` script inside that folder.
3. Select the supported target for your installation: `OG` (1.10.163) or `AE` (1.11.137+).
4. Enter your Mod Organizer 2 mods path (or game data folder) when prompted to automatically install the compiled plugin DLL and the universal HTML/CSS/JS UI assets.
5. Mod authors can easily use this folder as a boilerplate/template to start building their own custom PrismaUI mods.

## Requirements

- Fallout 4 runtime (Old-Gen 1.10.163 or Anniversary Edition 1.11.137+ with Address Library data)
- F4SE
- C++23 compiler (MSVC 2022 recommended)
- xmake 3.0+

## Credits and License

This project is a Fallout 4 (F4SE) conversion of the original **Prisma UI** framework by
**StarkMP**, forked and maintained by NomadsReach with StarkMP's permission. The original
SKSE/Skyrim framework lives at https://github.com/PrismaUI-SKSE/framework.

This software is distributed under the **Prisma UI License** (see [LICENSE.md](LICENSE.md)).
It is NOT MIT and NOT open source. In short:

- You may use it for non-commercial or commercial purposes.
- You may study it and modify it for private use.
- You may redistribute the original version only if this license is included without modification.
- You may NOT publicly distribute a modified version without written permission from the author.

### Ultralight dependency

Ultralight is separately licensed. Release packages include the unchanged Ultralight license,
EULA, notices, and a product credit under `Data/PrismaUI_F4/licenses/`. Distributors must confirm
that their use is covered by the Ultralight Free License or an applicable commercial license; that
eligibility and the sufficiency of downstream EULA terms are not determined by this package.
