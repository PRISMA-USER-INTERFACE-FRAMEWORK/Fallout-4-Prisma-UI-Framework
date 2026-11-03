---
id: validation-project-files-and-troubleshooting
title: Validation, Project Files, and Troubleshooting
sidebar_position: 8
description: Save reusable projects, understand validation findings, and diagnose common issues.
---

# Validation, Project Files, and Troubleshooting

Prisma Designer validates the project on canvas mutations and reports findings that could make an export silently wrong. Findings are visible and actionable, but validation does not prevent you from exporting.

## Save a project

Use **Save** or **Ctrl+S** to download a `.prisma` project. Older releases may also open a legacy `.json` single-view project. A project stores the canvas, elements, properties, themes, bindings, and visual scripting graph so you can reopen and continue editing it.

Use **Open** to load the project and **New** to clear the current canvas after the confirmation prompt. Keep the project file as the source of truth; exported HTML is the runtime artifact rather than the editable project format.

## Validation findings

Run through the validation panel before shipping. Pay special attention to:

- duplicate generated names or listener names;
- elements placed outside the intended canvas;
- invalid or incomplete visual scripting connections;
- missing values needed by a widget or binding;
- a solid background when transparency was intended.

Fix the source element or graph node and run validation again before shipping the export.

## Common issues

### The exported view is blank

Install the file under a plugin-specific directory below the Prisma views root, for example:

```text
Data/PrismaUI_F4/views/MyPlugin/menu.html
```

Pass the path **relative to `views/`** to `CreateView`:

```cpp
g_view = g_api->CreateView("MyPlugin/menu.html", OnDomReady);
```

Do not add an `Interface/` prefix unless `Interface` is intentionally part of your own plugin directory layout.

Also confirm that PrismaUI_F4 is loaded, the returned view handle is nonzero, and the view is created at a lifecycle point where your plugin is ready to own UI such as `kPostLoadGame` / `kNewGame`.

### Buttons do not do anything

The exported HTML provides the JavaScript call site, not your native game callback. Confirm that the listener name in the Button properties exactly matches the name passed to `RegisterJSListener` in your plugin. Register native listeners from the DOM-ready callback and test the exported view from the real consumer plugin, not only from Bridge preview.

### The game world is hidden behind a dark rectangle

Set the canvas background to `transparent` in Canvas Settings and export again. The canvas background is copied into the HTML body style.

### The Bridge preview will not connect

Confirm Fallout 4 is running with PrismaUI_F4 and PrismaDesignerBridge enabled. The browser editor still works without the Bridge, so use export and normal plugin loading while connection issues are resolved.

### A preset appears in the wrong place

The **Main Menu**, **Tactical HUD**, and **Inventory Screen** presets use literal full-canvas coordinates. Start with a new canvas at the preset's target resolution. Smaller component groups center around the current cursor position.

## Working on Prisma Designer

For development of Designer itself, use its provided server rather than `python -m http.server`:

```bash
python3 tools/dev-server.py
```

The Designer repository includes tests for logic compilation, export scope, generated names, and presets:

```bash
node tests/logic-compile.test.js
node tests/export-scope.test.js
node tests/generated-names.test.js
node tests/presets.test.js
```

Normal Designer users do not need Node, Python, or a build step. Return to [Getting Started](./getting-started) when you are ready to create another view.
