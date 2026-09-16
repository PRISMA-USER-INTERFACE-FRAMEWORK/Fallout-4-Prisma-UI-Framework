---
title: BGS Release Notes
---

# Behaviour Graph Studio Release Notes

This page is the public summary of the current BGS development cycle. It covers the work behind the 1.1 release, including the desktop workspace, authoring pipeline, animation and physics inspection, diagnostics, automation, and safety boundaries.

For installation and the first edit, start with [Getting Started](getting-started). For automation, see [MCP Integration](mcp) and the [MCP API Reference](mcp-api).

## 1.1.0: dependable authoring and diagnostics

BGS 1.1.0 focuses on making Fallout 4 HKX work easier to inspect, safer to edit, and easier to validate before it reaches the game.

### The Bridge workspace

- Added a Home/Bridge surface that puts the current file, common actions, tools, and orientation in one place.
- Added a first-run tour that can be replayed from the Bridge.
- Added recent-file shortcuts and a Bridge search for stations and reference entries.
- Added drag-and-drop opening for HKX files, including platform-safe single-file and multi-file drop payloads.
- Reorganized the application into Home, Graph, Inspect, Animation, and Project activities without removing the existing tools.
- Kept command-bar actions for Open, Save, Undo, Redo, Check graph, and Check project available from the main workflow.
- Added contextual secondary views so Tree, Graph, Symbols, Chain, Animation, Playback, Compare, and diagnostics remain reachable without hunting through menus.

### Behaviour graph inspection and authoring

- Browse real HKX objects in a hierarchical Tree view or a visual Graph view.
- Filter large files and focus the selected object and its connected path.
- Inspect scalar fields, structs, arrays, symbols, references, and object metadata.
- Edit supported fields while preserving unsupported or unknown data instead of guessing.
- Create, attach, edit, and remove supported states, transitions, generators, variables, events, bindings, and arrays.
- Author supported transition conditions, enter and exit notify events, global events, and non-empty state-machine structures.
- Copy graph subtrees between files and save reusable templates from known-good structures.
- Show symbol usage, variable bounds, event relationships, and project dependencies.
- Compare behaviour files with filtering and deterministic added/removed/changed results.
- Simulate event-driven state transitions, timing, blend weights, and active-state changes without pretending to be the Fallout 4 runtime.

### Animation, skeleton, and mesh work

- Inspect animation metadata, annotations, frame counts, timing, bones, and root-motion information.
- Preview animations on a skeleton and, when a compatible asset is available, a skinned mesh.
- Scrub frames directly, filter bones, inspect motion chains, and diagnose timing or pose changes.
- Edit supported animation frames, trim clips, and retime supported animation data.
- Resolve adjacent skeletons and matching mounted-turret meshes deterministically.
- Validate skeleton hierarchies and inspect retarget/skeleton-mapper data.
- Author simple explicit skeleton-mapper rows with measured validation and conservative refusal of malformed inputs.
- Inspect and edit supported skin influences and persist verified NIF weight changes.
- Report mesh/skeleton mismatch and rest-pose drift rather than displaying a misleading deformation as if it were valid.

### Ragdoll and physics inspection

- Inspect measured ragdoll bodies, shapes, constraints, bone bindings, body frames, pivots, limits, and centres of mass.
- Display engineering labels and animation-to-ragdoll mappings to make physics data easier to relate to the skeleton.
- Measure the distance between two selected body frames.
- Refuse self-pairs and stale selections instead of reporting a misleading measurement.
- Provide a constrained Drop/Recover engineering preview for measured ragdoll data.
- Keep lifecycle reset and stale-state protection around the preview controls.

The ragdoll work is intentionally an inspection and engineering-preview surface. It is not a full Havok solver, body-collision simulation, or complete ragdoll authoring system.

### Cloth boundary

BGS can inspect and validate bounded cloth structures and references, including particles, buffers, collidables, constraints, operators, states, and simulation-cloth relationships.

Cloth simulation, mesh deformation, cloth authoring, and cloth save/write-back are not implemented. The public tool reports these boundaries explicitly so inspection is not confused with a production simulation result.

### Archives and load-order diagnostics

- Browse vanilla HKX files directly from Bethesda BA2 archives without extracting an entire archive.
- Keep archive-opened files read-only temporary copies.
- Scan archives for corrupt or unreadable behaviour and character files.
- Scan enabled MO2 modlists using engine-style loose-file and archive precedence.
- Scan merged clip references for animations that are genuinely absent from the load order.
- Emit bounded, versioned JSON reports for CI and scripts.
- Return stable exit codes for clean scans, defects, and usage/configuration errors.
- Cache archive indexes and stream progress without decompressing files that only need existence checks.
- Resolve crash-log animation subgraph references against the game's own manifests and identify missing per-weapon clips.
- Compare vanilla and modded subgraph coverage.

These are static load-order diagnostics. They do not reproduce every runtime animation-database, binding, physics, or actor-state decision made inside Fallout 4.

### Validation and safe saving

- Run graph checks for broken references, missing objects, suspicious values, and unsupported structures.
- Run project checks across related behaviour files and animation references.
- Validate supported edits before saving.
- Save through the native verified pipeline instead of raw HKX/XML patching.
- Preserve the original as a `.bak` backup.
- Refuse unsupported, ambiguous, stale, or unverified writes before replacing the source.
- Detect another program changing the file during a save and stop rather than overwrite newer data.
- Recover interrupted BGS save transactions while preserving the replaced version as a backup.
- Reopen saved output and verify the result before treating it as complete.
- Keep Undo and Redo separate from disk publication so an approved change can still be cancelled before saving.

### Assistant and provider integration

- Added an optional right-side Assistant drawer rather than a separate activity that displaces editing work.
- Added an OpenAI-compatible provider boundary with configurable endpoint, model, and environment-variable credential name.
- Keep credentials out of source, project files, and prompts used for ordinary inspection.
- Send only the user's prompt, bounded editor context, and BGS tool results when a request is submitted.
- Support cancellation and sequential tool execution.
- Allow the Assistant to inspect the active document and prepare a supported clip change.
- Require explicit human approval in the BGS UI before an in-app clip mutation is applied.
- Bind the pending approval to the exact active-document revision, object, old value, and new value.
- Drop stale pending changes when the document or proposed change no longer matches.
- Preserve normal validation, source stamps, verified save, backup, and undo guarantees after approval.

### External MCP integration

The external stdio MCP server is bounded and read-only. It exposes seven tools:

1. `bgs.inspect_behavior`
2. `bgs.resolve_project_chain`
3. `bgs.check_project`
4. `bgs.search_project`
5. `bgs.inspect_animation`
6. `bgs.inspect_object`
7. `bgs.preview_set_clip_animation`

The preview tool never writes a file. External MCP has no write-capable tool, shell access, process control, arbitrary filesystem write, or caller-controlled approval path. Every request is limited to explicit `--root` directories, with canonical-path and link/reparse-point checks.

### Compatibility and known limitations

- Fallout 4 packfiles are the primary supported Havok target.
- Windows x64 and Linux x64 packages are self-contained release targets.
- Fallout 76 evidence currently covers Havok 2015.1.0, not a completed Havok 2018 implementation.
- Skeleton-mapper serialization/write-back and chain-mapper authoring remain incomplete.
- Automatic mapper bone pairing is not proven; simple authoring requires explicit pairs.
- Reciprocal mapper generation is a construction aid, not a universal symmetry claim.
- Full ragdoll authoring, body collision solving, and full Havok dynamics are not implemented.
- Cloth is inspection and validation only.
- Drop/Recover follows measured poses as an engineering preview; it is not Havok dynamics, body collision, cloth interaction, or an in-game substitute.
- Unsupported layouts, classes, and conversions remain read-only or are refused.

These limits are deliberate. BGS prefers a clear refusal or read-only result over writing data it cannot verify.

## Upgrade checklist

After installing a new release:

1. Confirm the version in About or with `--version`.
2. Open a copy of a known-good behaviour file.
3. Check the Tree and Graph views.
4. Preview one animation and confirm the expected skeleton or mesh is selected.
5. Run Check graph and Check project.
6. Make one supported edit, save, reopen, and confirm the `.bak` exists.
7. Compare the result with the original.
8. Test the changed behaviour in Fallout 4.
9. If using MCP, restart the client and confirm the seven-tool discovery list.

## Feedback

Report reproducible issues through the [BGS issue tracker](https://github.com/G-A-R-D-E-N/BGS/issues). Include the BGS version, platform, file type, steps to reproduce, validation output, and whether the file came from disk or a BA2 archive.
