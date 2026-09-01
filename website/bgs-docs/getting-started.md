---
title: Getting Started
---

# Getting Started

Behavior Graph Studio is a standalone desktop editor for Fallout 4 Havok `.hkx` files. It runs outside the game and lets you inspect, edit, validate, compare, and preview behaviour data without installing anything into your Fallout 4 folder.

This guide assumes you are new to Fallout 4 behaviour graphs. You do not need to understand every Havok class before you start. A good first goal is to open an existing behaviour, find one clip or state, make a small value change, validate it, save it, and test it in game.

## Requirements

- Windows 64-bit or Linux x64.
- No Havok Content Tools installation is required.
- No Java runtime is required for supported edits.
- No Fallout 4 game SDK is required.
- A Fallout 4 installation is useful if you want to inspect the vanilla archives, but the editor itself can run without the game installed.

## Download and run

1. Download the latest release from [Nexus Mods](https://www.nexusmods.com/fallout4/mods/107691) or [GitHub](https://github.com/NomadsReach/BehaviorGraphStudio/releases).
2. Extract the archive to a normal folder you control, such as `C:\Modding\BehaviorGraphStudio` or `~/Tools/BehaviorGraphStudio`.
3. Run `BehaviourGraphStudio.exe` on Windows or the Behaviour Graph Studio binary on Linux.

There is no installer and no setup wizard. Do not place the program inside the Fallout 4 `Data` folder. Keeping the editor separate from the game makes updates and troubleshooting much easier.

## Before editing a mod

For your first few edits, work on a copy of the file rather than the only copy in your project.

A simple workspace can look like this:

```text
MyBehaviourTest/
  original/
    MyBehavior.hkx
  working/
    MyBehavior.hkx
```

Open the copy under `working`. Behavior Graph Studio also creates a `.bak` file when saving, but keeping a clean source copy makes experimentation easier.

## What kinds of files can it open?

Behavior Graph Studio can inspect Fallout 4 behaviour, character, project, skeleton, and animation `.hkx` files. The main graph authoring workflow is centered on behaviour files, while the other file types help the editor resolve the surrounding project chain, skeleton, animation data, and playback context.

If a file is not a Fallout 4 Havok packfile, the editor refuses it instead of trying to interpret unrelated data as a behaviour graph.

## Opening a file from disk

Use **Browse...** or enter a path in the file field and press **Open**.

For mod development, this is the normal workflow. Open an extracted `.hkx` file from your mod project, make your changes, validate them, then save the file back into the mod's folder structure.

After a behaviour opens, the main window gives you these tabs:

- **Tree** for a hierarchical object list.
- **Graph** for the visual node graph and editing tools.
- **Symbols** for variables, events, bindings, and symbol usage.
- **Chain** for the project, character, behaviour, skeleton, and animation relationship.
- **Animation** for animation data and frame-level inspection.
- **Playback** for skeleton or supported mesh preview.
- **Compare** for comparing the open behaviour against another file.

## Opening a vanilla file from a BA2 archive

Use **From archive...** when you want to inspect a vanilla file without manually extracting the entire archive.

Most vanilla animation and behaviour content is stored in Bethesda `.ba2` archives. The archive browser lets you search the archive index and open a matching file directly.

A file opened from an archive is a **read-only temporary copy**. This is intentional. It prevents an archive inspection session from being mistaken for an editable mod file.

If you find a vanilla behaviour you want to modify:

1. Open it from the archive and inspect it.
2. Note the original path inside the archive.
3. Copy or extract the file into your own mod workspace while preserving the expected folder structure.
4. Open that writable copy from disk.
5. Make your changes there.

### Tip: search archives with short terms

When browsing a large archive, start with a distinctive actor, animation, or behaviour name instead of typing a full path. Searching for `dogmeat`, `turret`, or part of an animation name is often faster than guessing Bethesda's complete directory layout.

## A safe first edit

A clip playback value is a good first test because it lets you learn the editor without immediately changing the graph structure.

1. Open a behaviour file from a writable folder.
2. Use the filter box to search for a known animation name or `hkbClipGenerator`.
3. Select a clip in the **Tree** or **Graph** tab.
4. Look at its fields in the properties panel.
5. Change one supported value, such as playback speed or a timing value.
6. Press **Check graph**.
7. Review any reported problems.
8. Press **Save to .hkx**.
9. Confirm the `.bak` file was created beside your edited file.
10. Put the edited file in your mod and test the exact action you changed in Fallout 4.

## Recommended workflow

For larger edits, use this order:

1. **Find** the relevant object in Tree or Graph view.
2. **Understand** its incoming and outgoing references before changing structure.
3. **Edit** one logical piece at a time.
4. **Simulate or preview** the affected path when the feature applies.
5. **Check graph** after local graph edits.
6. **Check project** when the edit depends on files elsewhere in the behaviour project.
7. **Save** only after the validation results make sense.
8. **Test in game** before stacking several unrelated changes on top.

Small, testable steps make behaviour work much easier to debug.

## Helpful habits

- Use **Ctrl+Z** for Undo and **Ctrl+Y** for Redo while experimenting.
- Keep the `.bak` file until the edited behaviour has been tested in game.
- Use the filter box aggressively. Large behaviour files can contain hundreds or thousands of objects.
- When changing a transition, inspect the source state, destination state, event, conditions, and generator path together.
- If something looks wrong after an edit, compare the file against your original rather than trying to remember every field you touched.
- Do not treat every validation warning as a fatal error. Some warnings describe unusual but valid graph layouts. Read the explanation and confirm whether it matches the design of the behaviour.

Next, see [Tree & Graph View](tree-and-graph-view) for finding your way around a loaded behaviour.
