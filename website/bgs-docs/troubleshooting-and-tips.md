---
title: Troubleshooting & Tips
---

# Troubleshooting & Tips

Most problems in behaviour editing fall into one of four groups: the wrong file is open, a reference cannot be resolved, the graph edit is logically incomplete, or the save pipeline is refusing something unsafe.

Work through the problem in that order before assuming the file is corrupted.

## Save is disabled

First check how the file was opened.

Files opened directly from a `.ba2` archive are read-only temporary copies. They are meant for inspection.

To edit one:

1. Preserve the original path shown by the archive browser.
2. Copy or extract the file into a normal mod workspace.
3. Open the extracted copy from disk.
4. Confirm **Save to .hkx** is available after making a supported edit.

## The editor refuses to open a file

Behavior Graph Studio expects Fallout 4 Havok packfiles for its HKX workflows.

If a file has the wrong extension or its contents are not a supported Fallout 4 packfile, the editor refuses it instead of attempting a dangerous parse.

Check:

- The file really came from Fallout 4 or a Fallout 4 mod.
- It is not a Skyrim HKX from a different Havok version.
- It is not XML that was renamed to `.hkx`.
- The file was fully extracted and is not truncated.
- The file has not been replaced by an archive stub, zero-byte file, or download error.

## I cannot find the node I need

Use the filter box before manually expanding the entire tree.

Try searching by:

- Animation filename.
- Part of an actor name.
- Havok class.
- State name.
- Event name.
- A distinctive word such as `reload`, `attack`, or `idle`.

If the graph is too dense, select the result and use **Fit selection** or highlight that object's paths.

## The graph looks overwhelming

Do not try to read a large behaviour as one diagram.

Use this workflow:

1. Start from something you recognize, usually a clip, event, or state name.
2. Filter for it.
3. Select it.
4. Highlight its paths.
5. Follow one connection at a time.
6. Use the field label on each edge to understand why the connection exists.

Tree view answers "what objects are here?" Graph view answers "how are these objects connected?" Switching between them is often faster than forcing one view to do everything.

## A transition does not fire

Check the whole transition, not only its destination.

Confirm:

- The source state is actually active.
- The destination state is correct.
- The event index or variable reference is correct.
- The required condition is true.
- Any timing gate has been satisfied.
- The destination state has a valid generator.

Then use simulation:

1. Fire the event or set the variable.
2. Step time forward.
3. Read the transition log.
4. Read **Held back** and **Stops** if the graph does not advance.

## The game crashes when loading the behaviour

Start with **Check graph**.

A state with no generator is a high-priority problem because the game can encounter it while walking the graph during load.

Also check for:

- Broken references after deleting or reconnecting objects.
- A transition pointing to a state that no longer exists.
- A structural edit that was only partially completed.
- An unexpected difference introduced by another tool or merge.

Compare the edited file against the original and inspect every structural difference around the affected state machine.

If you have a `.bak`, restore it and confirm the crash disappears before continuing. That tells you whether the behaviour edit is actually the cause.

## The animation is correct but the behaviour is wrong

Separate animation data from behaviour logic.

If the animation previews correctly by itself, inspect:

- Clip playback speed.
- Crop or timing fields.
- State transition timing.
- Which state is actually active.
- Whether another clip or generator is being selected at runtime.

Do not keep modifying animation frames to compensate for a state-machine problem.

## The skeleton works but no mesh appears

This usually means the animation context is available but mesh discovery or skin compatibility is not.

Check:

1. The **Chain** tab resolves the expected skeleton.
2. The intended NIF exists in the actor or project folders.
3. The mesh is skinned rather than only a static object.
4. Its bone names are compatible with the resolved skeleton.
5. There are not several equally plausible mesh candidates.

If skeleton playback works, continue debugging the mesh separately. The missing mesh does not by itself prove that the animation is bad.

## The wrong mesh is selected

Make sure the project context points at the actor or object you intended to inspect.

If several NIF files share many compatible bones, automatic selection can become ambiguous. Prefer a clearly matching body mesh over a weapon, accessory, collision-only NIF, or unrelated skinned object.

If the editor reports ambiguity, treat that as safer than silently accepting a wrong candidate.

## Check project reports missing animations

Confirm the relative animation path exactly as the behaviour stores it.

Common causes include:

- The animation exists but is in the wrong folder.
- A renamed file was not updated in the clip generator.
- A mod manager virtual path differs from the physical project layout you opened.
- The project, character, or skeleton file needed to resolve the chain is missing.

Use the **Chain** tab to find the first missing link instead of checking every file manually.

## A save is refused

A refused save is not the same as a failed or corrupted save. It means the editor stopped before replacing the source file.

Read the reported field and reason.

Then:

1. Undo the last edit.
2. Confirm the file is saveable again.
3. Recheck the expected field type and value range.
4. Try the intended change in a smaller step.
5. Run **Check graph**.
6. Report a reproducible case if the edit should be supported but is still refused.

Do not remove the `.bak` while investigating a save problem.

## The saved file behaves differently than expected

Use **Compare** against the original.

Unexpected behaviour usually becomes easier to understand once you can see every field and object that changed.

Look for:

- A second field changed accidentally.
- A symbol index shifted after removal.
- A transition destination changed with the intended edit.
- A copied subtree brought in event or variable references from another context.
- A timing change affected more than one clip or state.

## A template does not fit the destination file

Templates can contain assumptions about the source graph, including event and variable names.

Before applying one to a different file:

- Compare the required symbols.
- Confirm the destination object has the expected slot or reference field.
- Check whether the template assumes a generator or modifier class that does not exist in the destination path.

After insertion, validate before making more edits.

## Undo vs backup vs original copy

These protect different stages of your work:

- **Undo/Redo** protects the current editing session.
- **`.bak`** protects the file that existed immediately before a save.
- **Your original source copy** protects the clean starting point for the entire task.

For important work, keep all three until testing is complete.

## Tips for reverse engineering an unfamiliar behaviour

A reliable order is:

1. Find a recognizable animation clip.
2. Identify the state that owns or reaches it.
3. Highlight the state's paths.
4. Inspect incoming and outgoing transitions.
5. Open **Symbols** for the related events and variables.
6. Use simulation to exercise the path.
7. Open **Chain** to confirm external files.
8. Use **Compare** when studying a modded version against vanilla.

This is usually faster than starting at the root and reading every object in order.

## Tips for making a new behaviour edit

- Make the smallest change that proves your idea first.
- Validate after each structural step.
- Use descriptive names for new variables, events, and templates.
- Test state entry and state exit, not only the animation in the middle.
- Keep event names consistent with the rest of the file.
- Preview animations before changing graph logic to compensate for a bad asset.
- Compare against the original before release.
- Keep backups until the mod has been tested in Fallout 4.

## Reporting a bug

If the problem appears to be in Behavior Graph Studio itself, report it through [GitHub Issues](https://github.com/NomadsReach/BehaviorGraphStudio/issues).

Include as much of this as you can:

- Operating system.
- Behavior Graph Studio version or release.
- File type and Havok class involved.
- Exact steps that reproduce the problem.
- The exact error or refusal text.
- Whether the file came from disk or a BA2 archive.
- Whether the same problem happens after reopening the original file.
- A minimal test file when you are allowed to share it.

For the full save workflow, return to [Saving & Validating](saving-and-validating).
