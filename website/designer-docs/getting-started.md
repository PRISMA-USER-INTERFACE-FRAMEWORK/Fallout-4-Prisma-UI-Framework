---
id: getting-started
title: Getting Started
sidebar_position: 1
description: Install and open Prisma Designer, then export your first PrismaUI F4 view.
---

# Getting Started

Prisma Designer is a standalone visual editor for building PrismaUI F4 menus,
HUDs, Pip-Boy screens, and cursor views. It runs directly from `index.html`.
There is no installer, Node dependency, build step, or server required for the
normal workflow.

## Download and launch

1. Download the [Prisma Designer repository](https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Prisma-Designer) and unzip it anywhere.
2. On Windows, run **Launch PrismaDesigner.bat**.
3. On macOS or Linux, run `./launch-prismadesigner.sh`.
4. If you prefer, open `index.html` directly in Chrome or Edge.

The launchers only open the same `index.html` file in a browser. They do not
install anything. Chrome and Edge are the supported browsers for the editor.

## The first view

1. Click **New** in the top toolbar.
2. Choose a view type: **HUD**, **Menu**, **Pip-Boy**, or **Cursor**.
3. Click a component in the left palette to add it to the canvas.
4. Select the component and edit it in the right inspector.
5. Click **Export HTML**, then **Download**.

The exported file is a self-contained HTML view. Place it in a plugin-specific
directory under the PrismaUI views root:

```text
Data/PrismaUI_F4/views/<YourPlugin>/your-view.html
```

Then load it from your F4SE plugin with the path relative to
`Data/PrismaUI_F4/views/`:

```cpp
PrismaView view = api->CreateView(
    "YourPlugin/your-view.html",
    [](PrismaView v) {
        // Register listeners and invoke initialization here.
    });
```

PrismaUI_F4 2.1.0 does not require an extra `Interface` subdirectory under the views root.

The export dialog also has a **C++ Code** tab. It prints the `CreateView` and
`RegisterJSListener` calls for the listeners configured in the design.

## The interface

| Area | Purpose |
| --- | --- |
| Top toolbar | New, Open, Save, undo/redo, zoom, canvas settings, visual scripting, and export |
| Left sidebar | Widgets, templates, icons, and presets |
| Center canvas | Design surface at the target screen resolution |
| Right sidebar | Scene outliner and Properties, Events, and CSS inspector tabs |
| View tabs | Multiple views in the same project |

Continue with [Canvas and View Types](./canvas-and-view-types) to set up the
design surface, or jump to [Exporting HTML and C++ Integration](./exporting-and-cpp-integration)
for the complete game integration workflow.
