---
id: properties-events-and-binding
title: Properties, Events, and Data Binding
sidebar_position: 4
description: Configure visual properties, C++ listeners, and live game data bindings.
---

# Properties, Events, and Data Binding

Select an element on the canvas or in the scene outliner to open its inspector.
Changes are applied to the canvas as you type and are recorded in undo history.

## Transform and appearance

All elements expose the basic transform fields **X**, **Y**, **W**, and **H**.
You can also set **Radius**, **Opacity**, and **Lock**. Locked elements remain
visible but cannot be moved or resized accidentally.

Panel, Progress, Image, and Divider elements add **Fill**, **Border**, **B.Color**,
**B.Width**, and **Box Shadow**. Text, Button, and Badge elements add **Content**,
**Color**, **Size**, **Font**, **Weight**, **Align**, **Spacing**, and **Line H.**

Specialized elements expose fields for their generated runtime behavior:

- **Progress**: Value, Fill, and Track.
- **Compass**: Heading, Span°, Tick Color, and Background.
- **Objective**: Label, Distance, and Type.
- **Stat Bar**: Label, Value, Segments, Fill, label color, and label size.
- **Image**: Fit mode and an embedded image selected with **Browse...**.

## Scene outliner

The outliner lists the elements in layer order and shows nested groups. Click a
row to select it. The visibility control hides an element from both the canvas
and export, while the lock control prevents canvas interaction. Rename an
element from the name field at the top of the Properties panel.

Use **Front**, **Back**, **Up**, and **Down** to control which elements render on
top of one another.

## Button listeners

For a Button, open the PrismaUI API section and enter a **C++ Listener Name**,
such as `onCloseMenu` or `onConfirm`. Enable **Close button** when the button
should be treated as a dismiss action. Prisma Designer shows the exact C++
registration call in the live code preview.

Register the listener after creating the view:

```cpp
api->RegisterJSListener(view, "onCloseMenu", [](const char*) {
    api->Unfocus(view);
    api->Hide(view);
});
```

The exported HTML adds the listener metadata and JavaScript stub. The callback
does not become a real game callback until your plugin registers the same name.

## Runtime updates

Compass and Stat Bar elements export update functions. The exact generated name
contains the element's internal ID, which is shown in the exported HTML:

```cpp
api->Invoke(view, "drawCompass_EL_ID(247)");
api->Invoke(view, "updateStatbar_EL_ID(60)");
```

Objective labels can be changed through the DOM or through a function you add to
your exported view. For recurring, high-frequency updates, use the PrismaUI
`InteropCall` API in your plugin rather than evaluating arbitrary JavaScript on
every frame.

## Data binding

The binding controls associate an element with game data such as
`player.health`, `player.ap`, `player.rads`, `player.level`, and `player.caps`.
Bindings are exported with the view and are populated by the bridge during live
preview or by your own plugin at runtime.

For branching behavior that changes more than a single displayed value, continue
with [Visual Scripting](./visual-scripting).
