---
title: Editing Nodes
---

# Editing Nodes

Behavior Graph Studio lets you make both simple field edits and larger structural changes. The safest way to work is to understand the existing references first, change one logical piece at a time, then validate before saving.

## The properties panel

Select a node in either **Tree** or **Graph** view to inspect its fields in the properties panel. Double-clicking a graph node jumps directly into its first editable field.

Depending on the Havok class, fields can include:

- Animation names and paths.
- Playback speed.
- Crop and timing values.
- State or event IDs.
- Flags and modes.
- Weights.
- Generator references.
- Transition data.
- Arrays and object references.

Empty fields are still shown when they are meaningful, so an unset value can be authored rather than disappearing from the UI.

## Making a field edit

1. Select the object you want to change.
2. Find the field in the properties panel.
3. Enter the new value.
4. Finish the edit by moving focus out of the field.
5. Check the status area for any refusal or validation message.
6. Run **Check graph** before saving.

If a value cannot be written safely, the save pipeline refuses the edit instead of silently corrupting the file.

### Tip: change one related group at a time

If you are tuning a clip, change its timing values and test them before also changing transitions, events, and graph structure. Small groups of related edits are much easier to diagnose if something behaves differently in game.

## Undo and redo

Use the toolbar buttons or keyboard shortcuts while experimenting:

- **Ctrl+Z**: Undo.
- **Ctrl+Y**: Redo.

The editor keeps an undo history for the current session. This is useful for trying a value, checking the result, and quickly backing out without reopening the file.

Undo is not a substitute for the `.bak` file created on save. Undo protects the current editing session, while the backup protects the last saved file on disk.

## Adding nodes

The **Edit tools** area in Graph view contains authoring controls for creating and inserting supported graph objects.

A typical workflow is:

1. Select the object that should own or reference the new object.
2. Open **Edit tools**.
3. Choose the type of object you want to create.
4. Review the destination field or slot the editor identifies.
5. Create the object.
6. Inspect the new object in the graph and properties panel.
7. Run **Check graph** before continuing.

Supported authoring includes states, transitions, generators, modifiers, and other graph structures handled by the native save pipeline.

If you intentionally create an unattached object, verify that you eventually connect it where it belongs. Unreachable or unused objects may be reported by validation.

## Connecting and disconnecting references

Graph editing is not only about creating objects. You can also change which object a supported reference points to.

Before reconnecting something, identify:

- The source object.
- The field that owns the reference.
- The current destination.
- The intended destination.
- Any other objects that also reference the old destination.

### Tip: use path highlighting first

Highlight the source object's path before changing an important connection. This gives you a clean view of the relationship you are about to modify and helps catch accidental cross-links.

## Editing states and transitions

State-machine work deserves extra care because several pieces can interact at once.

When editing a state, check:

- The state's generator.
- Incoming transitions.
- Outgoing transitions.
- Event IDs or names.
- Conditions.
- Transition timing.

A state with no generator is especially important because it can cause a load-time crash. **Check graph** is designed to catch this before you test in game.

When retargeting a transition, confirm the destination state and then use the graph simulation tools to exercise the event or condition if possible.

See [Compare & Simulation](compare-and-simulation) for a workflow that helps verify state-machine changes before saving.

## Copying and pasting a subtree

For repeated graph structures, you can copy a node and the objects it owns, then paste that subtree elsewhere.

A useful workflow is:

1. Select the root of the structure you want to reuse.
2. Copy the subtree.
3. Select the destination object.
4. Open **Edit tools**.
5. Choose the appropriate destination slot.
6. Use **Paste subtree**.
7. Inspect the resulting references.
8. Run **Check graph**.

Subtree copy is helpful when two states need a similar generator chain or when you are building repeated authoring patterns.

## Reusable templates

Templates let you keep a graph shape for later use, including across files.

A template can carry references to variables and events from the source file. When using it somewhere else, make sure the destination file has compatible symbols or update the new objects after insertion.

Good template candidates include:

- Repeated clip and modifier chains.
- Standard state layouts.
- Generator combinations you use frequently.
- Common transition structures.

### Tip: name templates by purpose

Use names that describe what the structure does, not where you first copied it from. `LoopedIdleWithExit` is easier to reuse later than `State47Copy`.

## Deleting nodes

Delete is reference-aware. If another object still points at the node, the editor can refuse the deletion and tell you what still references it.

If deletion is refused:

1. Read the reference information in the message.
2. Go to the referencing object.
3. Decide whether that reference should be removed, redirected, or preserved.
4. Make the reference change first.
5. Try the deletion again.

Do not remove references blindly just to make Delete succeed. A refusal is often telling you about an important relationship in the graph.

## Working with arrays

Some Havok objects use arrays for references, bounds, bindings, or repeated data. Behavior Graph Studio can safely grow supported arrays through its native pipeline.

When adding array entries:

- Check what each index means before inserting values.
- Keep symbol indices consistent with the Symbols tab.
- Re-run validation after changing the length or ownership of a reference array.

## Before saving a structural edit

For any edit that changes graph shape, use this checklist:

1. No required state is missing its generator.
2. New objects are connected where intended.
3. Deleted objects have no remaining references.
4. Transition destinations are correct.
5. Events and variables resolve to the intended symbols.
6. **Check graph** has been run after the final edit.
7. **Check project** has been run if external animation or project files are involved.
8. The edited file is being saved from a writable mod workspace, not a temporary archive copy.

For symbol editing, continue with [Symbols & Variables](symbols-and-variables). For writing the final file, see [Saving & Validating](saving-and-validating).
