# PrismaUI Example Plugin

This is a complete, ready-to-compile example of a Fallout 4 F4SE plugin that uses the PrismaUI framework to render an HTML/JS/CSS interface in-game.

## Supported runtimes

The example produces one DLL that allows the Address Library to validate the running executable.
It supports:

- Old-Gen `1.10.163`
- Anniversary Edition `1.11.137` or later, with Address Library data for that executable

Next-Gen `1.10.980–984` is refused at load until every engine site the plugin uses has a
Next-Gen answer. The UI files (`.html`, `.css`,
`.js`) are shared by both supported runtimes.

### How to Compile

We have provided a fully automated build script using `xmake`.

1. Double-click the `build-and-deploy.bat` file in this directory.
2. Enter your Mod Organizer 2 mods path (or game data folder) when prompted.
3. The script builds and deploys the single DLL plus its shared UI assets.

### Publishing to NexusMods

When uploading your mod, state the supported runtime families clearly. A FOMOD can still install
the shared UI assets, but it must not advertise the DLL for Next-Gen.

## File Structure

```
example-f4se-plugin/
├── src/
│   ├── main.cpp              — Plugin entry point and initialization
│   ├── keyhandler/           — Input event handling
│   └── PCH.h                 — Precompiled header
├── view/                     — Your HTML/JS/CSS frontend
├── build-and-deploy.bat      — Automated OG and AE build script
└── xmake.lua                 — Build configuration
```

## Integration with PrismaUI_F4

This plugin requires the PrismaUI_F4 framework to be installed:
- Framework DLL: `PrismaUI_F4.dll`
- Framework dependencies: pinned Ultralight 1.4 runtime + resources

Make sure you have deployed the framework before loading your plugin in-game.

## Taking part in the layout editor

The framework lets the player move, resize and hide every Prisma view from the Prisma Dock. A plugin
does not need to do anything for that. To give the view a title and a default rectangle in the
editor, copy `PrismaUI_F4_Modern_API.h` next to `PrismaUI_F4_API.h` and register it after the
view is created:

```cpp
#include "PrismaUI_F4_Modern_API.h"

static void RegisterLayout(PrismaView view)
{
    PRISMA_UI_FLAT_API::LayoutAPI layout{};
    if (!PRISMA_UI_FLAT_API::Discover<PRISMA_UI_FLAT_API::ApiFeature::Layout>(
            PRISMA_UI_FLAT_API::LayoutApiVersion, layout)) {
        return;
    }
    const PRISMA_UI_FLAT_API::LayoutRect fallback{1400.0f, 40.0f, 480.0f, 270.0f};
    layout.RegisterView(view, "Example panel", &fallback);
}
```

Rectangles are in a virtual 1920 by 1080 stage. A page that wants to react while the player edits
can listen for the `prisma-layout-edit` event on `window`. See `LAYOUT_API.md` in the framework.
This snippet is not part of the example build.
