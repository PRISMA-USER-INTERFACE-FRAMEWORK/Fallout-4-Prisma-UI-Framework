---
title: Tree & Graph View
---

# Tree & Graph View

Every loaded behaviour can be inspected in two main ways. The **Tree** tab is usually faster for locating exact objects. The **Graph** tab is better when you need to understand how states, generators, modifiers, and references connect.

Both views point at the same underlying objects. Selecting an object in either view lets you inspect and edit its fields.

## Tree view

The Tree tab presents the file as a nested object list. Depending on the object, rows can show:

- The Havok class, such as `hkbClipGenerator`, `hkbStateMachine`, or `hkbBlenderGenerator`.
- An animation path when a clip references one.
- The object's file offset for low-level cross-checking.
- Children owned or referenced by the selected object.

Use **Expand all** when you want a quick overview of the file. Use **Collapse all** before searching a very large hierarchy so the result is easier to read.

### When Tree view is best

Use Tree view when you already know what you are looking for, such as:

- A specific animation filename.
- A known Havok class.
- A state or generator name.
- An object you reached from a validation result.

It is also useful when the graph has many crossing connections and you only need to inspect one object at a time.

## Graph view

The Graph tab lays the same objects out as a node canvas.

The canvas is designed to answer questions such as:

- What owns this generator?
- What does this state point to?
- Which field creates this connection?
- Where does this transition go?
- What else depends on this object?

Edges come from real reference fields in the file and are labelled with the field responsible for the connection. That makes the graph useful for editing, not just visualization.

Clip nodes also surface useful information, including their animation path and non-default playback speed, so you can identify many clips without opening every properties panel.

## Moving around the graph

Two useful controls are available in the Graph view toolbar:

- **Fit all** frames the visible graph.
- **Fit selection** focuses the selected object and its related area.

If you lose track of where you are after panning or zooming, **Fit all** is the fastest reset.

### Tip: select first, then fit

For a large graph, search for the object in the filter box, select it, then use **Fit selection**. This is usually faster than manually navigating across the canvas.

## Filtering

The filter box is one of the most useful controls in the application. It can help narrow the view by name, class, or animation text.

Useful searches include:

```text
hkbClipGenerator
hkbStateMachine
reload
idle
dogmeat
```

Press Enter after filtering to jump to the first matching object.

### Search by intent, not only by filename

If you do not know the exact animation path, search for part of the action you are investigating. For example, `reload`, `attack`, `idle`, or an actor name can reveal related clips and states that would be hard to find by browsing manually.

## Highlighting one object's paths

Right-click a graph node and choose **Highlight the paths of...** to isolate its relevant path through the graph. Unrelated nodes and wires become less prominent so the selected route is easier to follow.

This is especially useful for state machines with many transitions.

Use path highlighting when you need to answer questions such as:

- Which states can lead here?
- Which generator is active under this state?
- Which transition leaves this state?
- Is this clip part of the path I am actually editing?

Press Escape or clear the highlight when you are finished.

## Understanding node relationships before editing

Before deleting, reconnecting, or replacing a node, inspect both sides of the relationship.

A useful checklist is:

1. What object owns this node?
2. Which field points to it?
3. Does anything else also reference it?
4. If it is a state, what generator does the state use?
5. If it is a transition, what event or condition makes it fire?
6. If it is a clip, which animation does it play?

This prevents the common mistake of changing the object you can see while missing another reference that still depends on it.

## Validation markers and problem navigation

When **Check graph** reports a problem, selecting the result can take you back to the related graph object. Use that instead of manually searching for the object ID.

A productive loop is:

1. Run **Check graph**.
2. Select a reported problem.
3. Inspect the node and its nearby path.
4. Make one correction.
5. Run **Check graph** again.

## Large graphs

Very large behaviour files can be visually dense even with automatic layout. Do not try to understand the entire file at once.

Instead:

- Filter to the system you care about.
- Highlight one path.
- Use **Fit selection**.
- Follow references outward from one known state or generator.
- Use Tree view for exact object lookup and Graph view for relationships.

Once you have found the object you need, continue with [Editing Nodes](editing-nodes).
