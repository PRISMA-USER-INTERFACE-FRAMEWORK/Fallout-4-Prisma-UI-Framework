---
id: visual-scripting
title: Visual Scripting
sidebar_position: 5
description: Build event-driven UI behavior that exports as plain JavaScript.
---

# Visual Scripting

Prisma Designer includes a Blueprint-style graph editor. It compiles the graph
to plain JavaScript inside the exported HTML. The exported view does not need a
runtime library or a separate scripting package.

## Open the editor

Click **Visual Scripting** in the top toolbar. The editor has four tabs:

- **Visual Scripting** for the node graph
- **Live Preview** for trying the current behavior
- **JS Code** for the generated JavaScript
- **C++ Code** for listener registration snippets

Use the right-click search to spawn nodes. Drag from a pin to another pin to
connect them, or drag a wire to empty canvas space to open the compatible-node
menu. Comment boxes keep larger graphs readable.

## Node groups

The available nodes include:

- **Events**: Click, Change, Game State, and Key
- **Flow Control**: Compare & Branch, Branch, and Sequence
- **Actions**: Show, Hide, Toggle, Set Text, Set Value, Call C++, Play Animation,
  Delay, and Print
- **Variables**: named values shared by graph nodes
- **Math and Compare**: pure nodes that feed values into actions and branches

Pins are typed. A connection is accepted only when the source and destination
types are compatible. Compile diagnostics appear in the graph canvas and point
at the node that needs attention.

## Practical workflow

1. Add an event node, such as **Click**.
2. Add an action, such as **Toggle** or **Set Text**.
3. Connect the event output to the action input.
4. Add a **Compare & Branch** node when behavior depends on a value.
5. Use **Live Preview** to test the result.
6. Fix any compile diagnostics before exporting.

The graph has its own undo and redo support, including copy and paste for
selected nodes. Keep the generated **JS Code** tab open when you need to see
what the exported view will execute.

## Calling C++

The **Call C++** action uses the listener name configured in the target element
or event. The corresponding `RegisterJSListener` call is shown in the **C++
Code** tab. The preview can call the generated stub, but only the plugin that
registers the listener can perform the real game-side action.

When the graph is ready, follow [Exporting HTML and C++ Integration](./exporting-and-cpp-integration).
