---
id: canvas-and-view-types
title: Canvas and View Types
sidebar_position: 2
description: Navigate the canvas and choose the resolution for a Prisma Designer view.
---

# Canvas and View Types

The canvas represents the final screen resolution. Element positions are
absolute pixels, so a component placed at `x: 100`, `y: 80` exports at that
position in the game view.

## View types and resolutions

Choose the view type from the toolbar before you start:

| View type | Default resolution | Use it for |
| --- | --- | --- |
| HUD | 1920 x 1080 | Health, AP, compass, objectives, and other overlays |
| Menu | 1920 x 1080 | Full-screen menus and screens |
| Pip-Boy | 1280 x 720 | Pip-Boy and PDA layouts |
| Cursor | 64 x 64 | Pixel-art cursor images |

Use **Canvas Settings** to change **Width**, **Height**, **Grid Size**, and the
canvas **Background** color. Set the background to `transparent` when the game
world should remain visible behind the exported interface.

## Navigation

| Action | How |
| --- | --- |
| Pan | Hold **Space** and drag, or drag with the middle mouse button |
| Zoom | Scroll the mouse wheel |
| Zoom in or out | **Ctrl +** and **Ctrl -** |
| Fit the view | **F**, **Ctrl+0**, or the Fit button |
| Toggle the grid | Backtick (`) or the Grid button |
| Toggle snapping | Snap button in the toolbar |
| Align and distribute | Use the toolbar alignment controls with multiple elements selected |

## Add and arrange elements

Click a palette item to drop it in the current viewport. You can also drag a
palette item onto the canvas. Right-click the canvas for additional actions.

Select elements directly or from the scene outliner. Hold **Shift** to select
more than one element. Drag to move a selection, use the arrow keys to nudge by
one pixel, and use **Shift + Arrow** to nudge by ten pixels. Drag one of the
eight handles around a single selected element to resize it.

Useful editing shortcuts:

| Shortcut | Action |
| --- | --- |
| **Ctrl+A** | Select all |
| **Escape** | Deselect |
| **Ctrl+D** | Duplicate, offset by 20 pixels |
| **Ctrl+C / Ctrl+V** | Copy and paste |
| **Delete / Backspace** | Delete selected elements |
| **Ctrl+Z** | Undo |
| **Ctrl+Y** or **Ctrl+Shift+Z** | Redo |

The history is unlimited within the current session. Use **Save** regularly so
the JSON project remains your editable source of truth.

Next, learn what each palette group can do in [Widgets, Templates, Icons, and
Themes](./widgets-templates-icons-themes).
