---
title: Animation & Playback
---

# Animation & Playback

Behavior Graph Studio can inspect animation data and preview supported animations on a resolved skeleton or skinned mesh. This is useful when a behaviour edit depends on timing, pose, root motion, or whether the expected animation asset is being selected at all.

## Animation vs Playback

The two tabs serve different jobs:

- **Animation** focuses on the animation data itself, including frames and animation-level editing tools.
- **Playback** focuses on seeing that animation move on a skeleton or supported mesh.

If you are debugging a behaviour clip, first confirm the clip points at the animation you expect. Then use the animation and playback tools to inspect the asset itself.

## Resolving the project chain first

Playback works best when the editor can resolve the behaviour's surrounding project files.

Check the **Chain** tab and confirm that the expected skeleton and animation paths are found. If the chain is incomplete, preview may not have enough information to build the correct actor context.

A useful order is:

1. Open the behaviour.
2. Find the clip you care about.
3. Confirm its animation path.
4. Open **Chain** and check the resolved skeleton and related files.
5. Open **Animation** or **Playback**.

## Inspecting animation frames

The Animation tab includes frame-level inspection tools. You can use them to move directly through an animation rather than relying only on real-time playback.

Frame inspection is useful for:

- Finding the exact frame where a pose changes.
- Checking whether a bone begins moving earlier or later than expected.
- Comparing a retimed animation against its original timing.
- Locating a bad frame without repeatedly replaying the entire clip.

When the animation contains many bones, use bone filtering to narrow the view to the part of the skeleton you are investigating.

### Tip: debug one bone chain at a time

For a hand problem, start with the hand, forearm, and upper-arm bones. For a foot-slide problem, start with the pelvis, thigh, calf, and foot chain. Looking at the full skeleton at once can hide a small but important motion error.

## Previewing on the skeleton

Skeleton preview is the most reliable starting point because it removes mesh and skinning variables from the problem.

Use skeleton playback when you want to answer:

- Does the animation actually move the expected bones?
- Is the pose itself correct?
- Does the animation loop where expected?
- Is the timing correct?
- Is root movement present?

If skeleton playback is correct but mesh playback looks wrong, the likely problem is the mesh, skin binding, or mesh selection rather than the animation frames themselves.

## Previewing on a skinned mesh

When a compatible NIF can be resolved, the Playback tab can show the animation on a skinned mesh.

Mesh preview is useful for checking deformation and for confirming that the animation makes visual sense on the intended actor or object.

The editor validates mesh candidates before selecting them. If several candidates are equally plausible, it can refuse to guess rather than silently choosing an arbitrary mesh.

### If no mesh appears

A missing mesh preview does not automatically mean the animation is broken.

Check these in order:

1. Confirm a skeleton is resolved.
2. Confirm the expected NIF exists in the project or actor context.
3. Check whether the NIF is actually skinned to bones that match the resolved skeleton.
4. Try the animation in skeleton view.
5. If skeleton playback works, treat mesh discovery as a separate problem.

See [Troubleshooting & Tips](troubleshooting-and-tips) for more detail.

## Root motion

Some animations move the actor through the world rather than only posing bones in place. Behavior Graph Studio can expose root-motion information so you can see that travel instead of mistaking it for unwanted drift.

Use root-motion inspection when working on:

- Forward or backward movement.
- Lunges.
- Creature movement cycles.
- Door or machinery sequences that translate a root object.
- Animations that look correct in place but move incorrectly in game.

If the preview provides a follow-travel option, use it when root motion would otherwise carry the actor out of the useful camera area.

## Trimming and retiming

Supported animation editing includes frame and timing changes such as trimming and retiming clips.

Before changing timing, note the original:

- Frame count.
- Duration.
- Start and end pose.
- Root-motion distance when applicable.

After the edit:

1. Scrub the first frame.
2. Scrub the last frame.
3. Play through the full animation.
4. Check for a visible pop at either end.
5. Check root motion.
6. Save the animation through the supported native path.
7. Reopen it and confirm the result still matches what you saw before saving.

### Tip: behaviour timing and animation timing are different layers

A clip generator can change playback speed or crop time without changing the underlying animation file. Editing the animation frames themselves changes the asset.

When diagnosing timing, determine which layer actually needs to change before editing both.

## Using preview with behaviour editing

A useful workflow for a clip problem is:

1. Find the `hkbClipGenerator` in the behaviour.
2. Confirm its animation path and playback settings.
3. Preview the animation itself.
4. If the animation is correct, return to the behaviour and inspect the surrounding state or transition timing.
5. If the animation is wrong, fix the animation data instead of compensating for it with unrelated graph changes.

This keeps behaviour logic and animation data from becoming tangled together.

## Playback is a diagnostic tool

Preview is extremely useful, but it is not the Fallout 4 runtime. The game still controls actor state, physics, event timing, animation graph context, and other systems around the file.

Use preview to catch obvious animation, skeleton, mesh, and timing problems early. Then validate the behaviour and perform an in-game test for the final result.

Next, see [Compare & Simulation](compare-and-simulation) for testing state-machine logic and reviewing file differences.
