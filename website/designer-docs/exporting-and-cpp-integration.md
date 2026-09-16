---
id: exporting-and-cpp-integration
title: Exporting HTML and C++ Integration
sidebar_position: 6
description: Export a self-contained PrismaUI F4 view and load it from an F4SE plugin.
---

# Exporting HTML and C++ Integration

Click **Export HTML** to open the generated view. You can copy it to the clipboard or click **Download** to save `prisma-view.html`. **Export All** writes every view in the current project.

## What the export contains

The output is a self-contained HTML view intended for **PrismaUI_F4 2.1.0 / Ultralight 1.4.0**:

- element positions are absolute pixel coordinates;
- colors, fonts, borders, and shadows are inline styles;
- images are embedded as base64 data URLs;
- Compass and Stat Bar elements include their own canvas/draw functions;
- buttons with listener names include `data-prisma-call` metadata and JavaScript stubs;
- visual scripting is compiled into a script block in the same file.

There is no separate Node runtime requirement for an exported view. Test the exported HTML in PrismaUI itself before shipping because a normal desktop Chrome preview is not identical to Ultralight 1.4.0.

## Install the view in a mod

Use a plugin-specific directory:

```text
Data/PrismaUI_F4/views/MyPlugin/menu.html
```

Create the view using a path relative to the Prisma views root.

The public API callback typedefs are plain function pointers, so use non-capturing callbacks/static functions rather than capturing C++ lambdas:

```cpp
static PRISMA_UI_API::IVPrismaUI10* g_api = nullptr;
static PrismaView g_view = 0;

static void OnCloseMenu(const char*)
{
    if (!g_api || !g_api->IsValid(g_view)) return;
    g_api->Unfocus(g_view);
    g_api->Hide(g_view);
}

static void OnDomReady(PrismaView view)
{
    g_api->RegisterJSListener(view, "onCloseMenu", OnCloseMenu);
    g_api->Invoke(view, "window.init && window.init()");
}

static void CreateMenu()
{
    if (!g_api || g_view) return;

    g_view = g_api->CreateView("MyPlugin/menu.html", OnDomReady);
    if (!g_view) return;

    g_api->SetViewRole(g_view, PRISMA_UI_API::ViewRole::kPanel);
    g_api->Hide(g_view);
}
```

Acquire `g_api` during/after `kGameDataReady`, null-check it, and create the view at a bounded lifecycle point such as `kPostLoadGame`, `kNewGame`, or the first point where your plugin actually owns the UI.

## Button listeners

For every listener name configured in Designer, register the same name in C++:

```cpp
static void OnConfirm(const char* payload)
{
    // Validate payload and update game state.
}

static void OnDomReady(PrismaView view)
{
    g_api->RegisterJSListener(view, "onConfirm", OnConfirm);
}
```

The exported page provides the browser-side call site. Game state remains owned by your native plugin.

## Transparency

Set the Designer canvas background to transparent when the view should float over the game.

```css
html, body {
  background: transparent;
}
```

For normal 2D views, this is page transparency. Advanced offscreen/mesh integrations have separate APIs such as `SetViewOffscreenBackground` and should be tested independently.

## Assets and networking

Designer exports are deliberately self-contained. Keep that property when extending them:

- bundle required scripts/styles/fonts/images locally;
- do not add required CDN/Google Fonts dependencies;
- put required HTTP/WebSocket/server work in C++ and bridge the data into the view.

For current framework behavior see the main [PrismaUI_F4 2.1.0 docs](/docs/prisma-2-1-release), [API reference](/docs/api-reference), and [view lifecycle](/docs/view-lifecycle).

For the Designer Bridge and live preview workflow, see [Live Game Preview with PrismaDesignerBridge](./live-game-preview).
