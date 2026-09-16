---
id: live-game-preview
title: Live Game Preview with PrismaDesignerBridge
sidebar_position: 7
description: Preview a Prisma Designer layout inside a running Fallout 4 game.
---

# Live Game Preview with PrismaDesignerBridge

PrismaDesignerBridge is an optional F4SE companion plugin. It connects the browser editor to a running Fallout 4 game so you can preview the current view without restarting the game.

The Bridge's WebSocket connection is **Designer tooling**, not the networking model available to ordinary PrismaUI_F4 2.1.0 views.

## Setup

1. Build or download the Bridge from the `PrismaDesignerBridge/` directory in the Prisma Designer repository.
2. Install the Bridge DLL as an F4SE plugin.
3. Start Fallout 4 with PrismaUI_F4 and the Bridge enabled.
4. Open Prisma Designer in a desktop browser.
5. Click **Game** or **Send to Game** in the top toolbar.

The editor connects when the Bridge's tooling endpoint is available. Its game-state feed can populate supported preview bindings while the preview is open.

## What the preview verifies

The Bridge creates or replaces a PrismaUI view and sends the current exported layout to it. Use it to check:

- screen resolution and scale;
- element positions and z-order;
- colors, fonts, borders, and transparency;
- canvas rendering for components such as Compass and Stat Bar;
- live values supplied through supported preview bindings.

Click **Send to Game** after editing to hot-reload the current design without relaunching Fallout 4.

## Important limitation

Bridge preview verifies the Designer layout path. It does not replace your consumer plugin's native integration and does not register your plugin's C++ listeners.

To verify real interaction, export the view to a plugin-specific Prisma directory such as:

```text
Data/PrismaUI_F4/views/MyPlugin/menu.html
```

Then load it from the consumer plugin with:

```cpp
g_view = g_api->CreateView("MyPlugin/menu.html", OnDomReady);
```

Register the real JS listeners in your plugin and test the final interaction there. That is the authoritative game-side check.

If the Bridge cannot connect, continue working in the browser and use normal export/plugin loading for validation. See [Validation, Project Files, and Troubleshooting](./validation-project-files-and-troubleshooting) for diagnostic steps.
