---
title: Symbols & Variables
---

# Symbols & Variables

The **Symbols** tab is where you inspect and manage the variables, events, and related symbol data used by a behaviour graph.

Symbols matter because many graph objects store an index rather than repeating a name. If a transition listens for an event or a modifier reads a variable, the symbol table is what gives that reference meaning.

## What the Symbols tab shows

The Symbols tab can show information such as:

- Variable or event name.
- Symbol index.
- Variable type.
- Initial value.
- Minimum and maximum bounds when present.
- Places where the symbol is referenced.
- How an event is used by graph objects.

Use it whenever you are editing conditions, event-driven transitions, bindings, or variable-driven modifiers.

## Adding a variable

When creating a new variable:

1. Open the **Symbols** tab.
2. Add the variable.
3. Give it a clear name.
4. Choose the correct type.
5. Set an initial value if the graph expects one.
6. Add bounds if the variable should operate within a defined range.
7. Connect or reference it from the graph objects that need it.
8. Run **Check graph** after the references are in place.

### Tip: copy the naming style already used by the file

Vanilla behaviours often follow their own naming conventions. Matching the local style makes the graph easier to understand later and reduces confusion when comparing your additions against the original file.

## Adding an event

Events are commonly used to trigger transitions or communicate that an action occurred.

A safe event workflow is:

1. Add the event in **Symbols**.
2. Give it a unique, descriptive name.
3. Note its assigned index.
4. Connect the event to the state, transition, generator, or modifier that raises or consumes it.
5. Inspect event usage to confirm the direction makes sense.
6. Use graph simulation to fire the event when possible.

See [Compare & Simulation](compare-and-simulation) for exercising event-driven paths without launching the game.

## Renaming symbols

Rename a variable or event from the Symbols tab rather than trying to change individual numeric references manually.

The editor updates supported symbol references so the graph remains consistent.

After a rename:

- Search the graph for the new name.
- Inspect the symbol's usage list.
- Run **Check graph**.
- If scripts or external files refer to the event by name, check those separately because they are outside the behaviour file.

## Changing a variable type or initial value

Changing a variable's initial value is usually straightforward, but changing its type can have wider consequences.

Before retyping a variable, inspect every place it is referenced. A modifier or condition expecting a boolean may not make sense if the symbol becomes a real number, and the editor cannot know the design intent of every custom graph.

### Tip: usage first, type second

Use the symbol usage list before changing a type. If the variable is referenced in many places, inspect those objects first instead of discovering incompatible assumptions after the save.

## Setting variable bounds

**Set bounds** gives a variable a minimum and maximum value.

Some Fallout 4 behaviour files have a `variableBounds` array that is empty or shorter than the full variable list. Behavior Graph Studio can grow that array safely when a later variable needs a bound.

Intermediate entries are initialized in a way that preserves the existing meaning of variables that did not previously have a bound.

After setting bounds, verify the actual minimum and maximum shown in the Symbols tab before saving.

## Removing a variable or event

Removing a symbol can affect every symbol index above it. Behavior Graph Studio updates supported references so the table remains consistent, but you should still inspect the result carefully.

Before removing a symbol:

1. Expand or inspect its usage.
2. Confirm nothing important still depends on it.
3. Remove or retarget those graph references first when appropriate.
4. Remove the symbol.
5. Run **Check graph**.
6. Compare against the original if the change affected many indices.

## Inspecting event usage

Expanding an event shows where the behaviour interacts with it. Depending on the graph, a reference may indicate that the event is:

- Raised or sent.
- Listened for or consumed.
- Stored in a field where the direction is not automatically known.

Each result identifies the class and member involved so you can jump from a symbol name back to the object that actually uses it.

This is one of the fastest ways to understand an unfamiliar state machine. Instead of reading the entire graph, start with a distinctive event and follow its users.

## Bindings

Bindings connect variables and object members so data can drive behaviour at runtime.

When editing bindings:

- Confirm the variable type matches the destination member.
- Check whether the binding is intended to read from or write to the graph property.
- Inspect the variable's initial value and bounds.
- Use graph simulation where possible to see whether changing the variable affects the expected path.

## The Chain tab

The **Chain** tab shows the surrounding project context for the open file, including the path from project to character to behaviour to skeleton and animations.

Use it to answer questions such as:

- Did the editor find the expected character file?
- Which skeleton belongs to this behaviour?
- Are referenced animation files present?
- Is a required part of the project chain missing?
- Which bones are available on the resolved skeleton?

This is especially useful before animation preview or project-wide validation.

## Symbols and external scripts

Behavior Graph Studio can update references inside the Havok file, but it cannot automatically rewrite every Papyrus script, F4SE plugin, or external system that may rely on a custom event name.

If you rename or remove an event that your mod uses outside the behaviour file, search your mod project for that name as a separate step.

## A useful debugging workflow

If a transition does not fire when expected:

1. Find the transition in Graph view.
2. Identify its event or variable condition.
3. Open **Symbols** and inspect that symbol's usage.
4. Confirm the event or variable index is correct.
5. Open the simulation controls.
6. Fire the event or set the variable.
7. Step the graph and read the transition log.
8. Check any **Held back** or **Stops** explanation.

For animation inspection, continue with [Animation & Playback](animation-and-playback). For state-machine debugging, see [Compare & Simulation](compare-and-simulation).
