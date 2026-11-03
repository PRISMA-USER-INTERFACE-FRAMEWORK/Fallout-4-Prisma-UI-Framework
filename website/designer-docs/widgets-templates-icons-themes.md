---
id: widgets-templates-icons-themes
title: Widgets, Templates, Icons, and Themes
sidebar_position: 3
description: Choose Prisma Designer components, presets, icons, and Fallout-themed palettes.
---

# Widgets, Templates, Icons, and Themes

The left sidebar is organized around the things you place on a view. Start with
individual widgets when you need precise control, or use a preset as a starting
point and edit its elements afterward.

## Components

Standard components cover the usual layout needs:

- **Panel** for containers and backgrounds
- **Text**, **Heading**, and **Badge** for labels and status copy
- **Button** for clickable controls
- **Progress** for HP, AP, charge, and other fill bars
- **Image** for embedded artwork
- **Divider** for horizontal or vertical separators

Tactical components are designed for game HUDs:

- **Compass** renders cardinal labels and degree ticks.
- **Objective** renders a diamond waypoint marker with label and distance.
- **Stat Bar** renders a segmented bar such as HP, AP, ammo, or rads.

Additional components include inventory grids, character titles and renders,
resistances, S.P.E.C.I.A.L. blocks, tab groups, list views, sliders, toggles,
text inputs, key/value rows, and dropdowns.

## Presets

The built-in presets provide complete starting layouts:

| Preset | Starting point |
| --- | --- |
| Terminal Panel | Green phosphor terminal panel |
| Pip-Boy Panel | Pip-Boy screen with an HP bar |
| Modern Panel | Dark teal panel with a badge |
| HUD Overlay | Minimal HP and AP overlay |
| Tactical HUD | Compass, segmented bars, objective marker, and ammo |
| Main Menu | Full 1920 x 1080 menu with title, navigation buttons, and listeners |

The **Main Menu** preset is placed at absolute canvas coordinates and is sized
for 1920 x 1080. Click **New** before adding it so its source coordinates line
up with the canvas.

## Icons

The **Icons** tab contains 4,180 searchable SVG icons. Drag an icon onto the
canvas or use it inside an Image element. Images are embedded as base64 data
when you export, so the final HTML does not depend on the original file path.

## Templates and themes

Use the **Templates** tab for reusable groups such as terminals, HUD overlays,
main menus, and tactical panels. You can save a selection as a custom template
for use in another project.

The **Theme** controls provide named palettes such as Pip-Boy Green, Amber, and
Vault Blue. Applying a theme updates the design's named colors while preserving
the element layout.

The [Properties, Events, and Data Binding](./properties-events-and-binding)
guide explains how to turn these visual components into interactive game UI.
