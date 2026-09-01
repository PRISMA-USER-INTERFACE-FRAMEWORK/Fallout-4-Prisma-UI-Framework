---
title: Saving & Validating
---

# Saving & Validating

Validation is part of the normal editing workflow, not something to save for the end. Run checks after meaningful graph changes so a problem can be traced back to the edit that introduced it.

## Check graph

Use **Check graph** for problems inside the currently open behaviour.

It can identify issues such as:

- A state with no generator.
- Broken or invalid references.
- Unreachable states that may deserve review.
- Graph structures that do not match the expected relationships.

A state with no generator is especially important. Fallout 4 can encounter that state while loading the behaviour rather than only when gameplay reaches it, so it can become a load-time crash.

When a problem appears, select it in the results to return to the related graph object.

### Recommended loop

1. Make one logical edit.
2. Run **Check graph**.
3. Open the reported object.
4. Decide whether the result is a real problem or an intentional graph layout.
5. Fix it if needed.
6. Run **Check graph** again.

Do not ignore a warning simply because the file still saves. Validation is there to catch graph-level mistakes that may only become visible in game.

## Check project

Use **Check project** when the behaviour depends on files outside the current `.hkx`.

Project validation can inspect the surrounding project chain and catch problems such as missing or unresolved animation files and related external dependencies.

Run it after changes that involve:

- Animation paths.
- Character or project relationships.
- Skeleton-dependent playback.
- New or retargeted clips.
- Anything that worked inside the graph but may point at a missing external file.

### Check graph vs Check project

A simple rule is:

- **Check graph** asks whether the current behaviour makes sense internally.
- **Check project** asks whether the wider set of files it depends on can be resolved.

For a release candidate, run both.

## Saving

Use **Save to .hkx** when the file is writable and the edit is ready.

Supported field and structural edits go through the built-in native save pipeline. You do not need Java or Havok Content Tools to save supported changes.

Before replacing the source file, the editor performs safety checks. If it cannot represent an edit safely, it refuses the save and leaves the source file unchanged.

A refusal should be treated as useful information. Read the field and reason reported by the application instead of trying to force the same edit through another path.

## Automatic backup

Saving creates a `.bak` copy beside the edited file.

For example:

```text
MyBehavior.hkx
MyBehavior.hkx.bak
```

Keep that backup until you have tested the edited file in game.

If the new file causes a problem, restore the backup by moving the edited file aside and putting the `.bak` copy back under the original filename.

## What happens during a structural save

Simple value changes can often be written without rebuilding the entire graph. Structural edits can require more work because objects, arrays, or references may need to grow or move.

The native save pipeline validates the result before replacing the source file. The goal is to refuse an unsupported transformation rather than producing a file that looks saved but has silently lost objects or references.

This is why a save refusal is safer than a partially successful write.

## If Save is disabled

The most common reasons are:

- The file was opened directly from a `.ba2` archive and is read-only.
- No writable behaviour file is currently loaded.
- The current file type is being inspected rather than edited.
- The editor has determined that the current operation cannot be saved through the supported path.

For an archive file, copy or extract it into your own mod workspace and reopen that copy from disk.

## If a save is refused

Do not overwrite the source with external tools immediately. First record what the editor refused.

Use this process:

1. Read the exact field and reason in the status message.
2. Undo the last edit and confirm the file becomes saveable again.
3. Check whether the value or structure you entered matches the type expected by that Havok member.
4. Try a smaller version of the change if appropriate.
5. Run **Check graph**.
6. If the refusal still appears and the edit should be supported, report it with the source file type, object class, field name, and steps to reproduce.

## Testing in Fallout 4

Validation reduces risk, but the game is still the final runtime test.

For important behaviour changes:

1. Test on a disposable save when possible.
2. Reproduce the exact animation or state you changed.
3. Test entering and leaving the affected state.
4. Test the action several times, not only once.
5. Check related transitions and fallback states.
6. If the graph is actor-specific, test the intended actor and any important variants.

For structural changes, avoid combining several unrelated experiments into one file before the first in-game test.

## Comparing before release

Before shipping a larger edit, use the **Compare** tab against your original file.

You should be able to explain every reported difference. Unexpected changes are a reason to stop and inspect the file before release.

See [Compare & Simulation](compare-and-simulation) for a detailed comparison workflow.

## Release checklist

Before putting an edited behaviour into a mod release:

1. **Check graph** reports no unexplained errors.
2. **Check project** reports no unexpected missing dependencies.
3. The file saves without refusal.
4. The `.bak` exists and the original source is still recoverable.
5. **Compare** shows only changes you intended.
6. Animation preview looks correct when applicable.
7. State-machine simulation behaves as expected when applicable.
8. The edited behaviour has been tested in Fallout 4.
9. The mod contains the file at the correct relative path.

If something goes wrong, see [Troubleshooting & Tips](troubleshooting-and-tips).
