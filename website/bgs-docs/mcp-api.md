---
title: MCP API Reference
---

# MCP API Reference

This page documents the public external stdio MCP surface shipped by Behaviour Graph Studio. All seven tools accept a path to a file inside an explicitly configured `--root`. Results are bounded and structured.

## Tool list

| Tool | Effect |
| --- | --- |
| `bgs.inspect_behavior` | Read behaviour structure, diagnostics, class counts, and round-trip findings. |
| `bgs.resolve_project_chain` | Read the project, character, skeleton, and animation relationships around an HKX. |
| `bgs.check_project` | Read bounded project-wide validation findings. |
| `bgs.search_project` | Search bounded behaviour, symbol, field, and asset matches. |
| `bgs.inspect_animation` | Read animation metadata, annotations, and bone names without frame transforms. |
| `bgs.inspect_object` | Read one HKX object, its fields, list summaries, and reference sites. |
| `bgs.preview_set_clip_animation` | Preview one clip animation-name replacement without writing. |

There is no external `set`, `save`, `write`, `patch`, shell, or filesystem mutation tool.

## `bgs.inspect_behavior`

Inspect one behaviour HKX with bounded graph and round-trip diagnostics.

```json
{
  "path": "C:\\Modding\\MyMod\\MyBehavior.hkx",
  "findingLimit": 100
}
```

Parameters:

- `path` string, required: an HKX file inside an allowed root.
- `findingLimit` integer, optional: maximum findings to return.

Use this first when you need to know whether a file can be read and which classes, objects, links, and validation issues it contains.

## `bgs.resolve_project_chain`

Resolve the bounded project chain for an HKX.

```json
{
  "path": "C:\\Modding\\MyMod\\MyBehavior.hkx",
  "animationLimit": 100
}
```

Parameters:

- `path` string, required.
- `animationLimit` integer, optional: maximum animation-source entries.

The result identifies related files and reports whether referenced animations are available through the resolved chain. It does not modify or extract files.

## `bgs.check_project`

Check the behaviour files belonging to a project.

```json
{
  "path": "C:\\Modding\\MyMod\\MyBehavior.hkx",
  "fileLimit": 100,
  "findingLimitPerFile": 50
}
```

Parameters:

- `path` string, required: the starting HKX.
- `fileLimit` integer, optional: maximum project files to inspect.
- `findingLimitPerFile` integer, optional: maximum findings per file.

Use this as a project-level gate after inspecting a single file. A clean result is not a substitute for an in-game test.

## `bgs.search_project`

Search bounded project data for a query.

```json
{
  "path": "C:\\Modding\\MyMod\\MyBehavior.hkx",
  "query": "hkbClipGenerator",
  "limit": 100
}
```

Parameters:

- `path` string, required.
- `query` string, required: a class, object, field, symbol, animation, or asset search term.
- `limit` integer, optional: maximum matches.

Start with a distinctive term such as an object name or animation name, then use `bgs.inspect_object` on a returned object identifier.

## `bgs.inspect_animation`

Inspect animation metadata and annotations without returning frame transforms.

```json
{
  "path": "C:\\Modding\\MyMod\\Animations\\Walk.hkx",
  "annotationLimit": 100,
  "boneLimit": 100
}
```

Parameters:

- `path` string, required.
- `annotationLimit` integer, optional.
- `boneLimit` integer, optional.

Use the desktop Playback tab for visual motion, mesh deformation, root travel, and frame-by-frame pose inspection.

## `bgs.inspect_object`

Inspect one bounded object in an HKX.

```json
{
  "path": "C:\\Modding\\MyMod\\MyBehavior.hkx",
  "objectId": "42",
  "fieldLimit": 100,
  "referenceLimit": 100
}
```

Parameters:

- `path` string, required.
- `objectId` string, required: the object identifier returned by another inspection.
- `fieldLimit` integer, optional.
- `referenceLimit` integer, optional.

The result separates scalar fields, lists, structs, outgoing references, incoming references, and bounded element summaries. It is intended for focused follow-up inspection, not a replacement for the graph UI.

## `bgs.preview_set_clip_animation`

Build a side-effect-free preview of changing one clip's `animationName`.

```json
{
  "path": "C:\\Modding\\MyMod\\MyBehavior.hkx",
  "objectId": "42",
  "animationName": "Animations\\MyMod\\Walk.hkx"
}
```

Parameters:

- `path` string, required.
- `objectId` string, required.
- `animationName` string, required.

The result reports whether the proposed change is accepted, the source identity, validation delta, and any blocking findings. It never writes the HKX, creates a backup, or changes the source file.

To apply a supported change, open the file in BGS. The in-app Assistant can stage the change, but a person must approve it in the UI before the verified save pipeline can run.

## Error codes

Common path and input errors are returned as structured error results:

- `invalid_argument`: a required path or argument is missing.
- `path_not_found`: the requested file does not exist.
- `path_not_file`: a directory was supplied where a file is required.
- `path_not_allowed`: the canonical file path is outside every configured root.
- `path_link_not_allowed`: a symbolic link or reparse-point component was detected.

The server also reports parser and validation failures without guessing at unsupported data. Treat an error result as a reason to inspect the file or workspace, not as permission to widen the root or bypass validation.

## Calling convention

The server uses MCP JSON-RPC over stdin/stdout. Do not write logging or other text to stdout from a wrapper around the server. Keep diagnostics on stderr and let the MCP client handle initialization, tool discovery, and calls.

See [MCP Integration](mcp) for installation, client configuration, root scoping, and a complete first-run workflow.
