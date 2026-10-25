---
title: MCP Integration
---

# MCP Integration

Behaviour Graph Studio includes an optional native [Model Context Protocol](https://modelcontextprotocol.io/) server. It lets an external MCP client inspect Fallout 4 Havok files and project relationships using bounded, structured results.

The external server is deliberately read-only. It does not expose a write-capable tool, shell access, process control, or arbitrary filesystem writes. The only change-shaped operation is `bgs.preview_set_clip_animation`, which computes a proposed clip change without writing a file.

## What MCP is useful for

MCP is a good fit when you want an assistant or automation client to:

- inspect a behaviour graph without opening the desktop window;
- list project-chain relationships and unresolved animation references;
- check a project and return bounded validation findings;
- search objects, fields, symbols, and asset references;
- inspect animation metadata, annotations, and bones;
- inspect one object with bounded fields and reference sites;
- preview a possible clip animation change before deciding what to do in BGS.

MCP is not a replacement for the BGS editor. Use the desktop application for editing, approval, validation, saving, undo, backups, and in-game testing.

## Requirements

- A BGS release for your platform, from [BGS releases](https://github.com/G-A-R-D-E-N/BGS/releases).
- An MCP client that supports local stdio servers.
- One or more directories containing the HKX files you want the client to inspect.

The server needs no API key and does not connect to a model provider. Your MCP client owns the conversation and model connection.

## Start the server manually

From the folder containing the BGS executable, run:

```powershell
.\BehaviourGraphStudio.exe --mcp --root "C:\Modding\MyProject"
```

On Linux:

```bash
./BehaviourGraphStudio --mcp --root "$HOME/modding/MyProject"
```

The server communicates over standard input and standard output. Logs and startup errors go to standard error so the JSON-RPC stream remains usable by the MCP client.

At least one `--root` is required. Repeat the option to allow multiple directories:

```powershell
.\BehaviourGraphStudio.exe --mcp `
  --root "C:\Games\Fallout 4\Data" `
  --root "C:\Modding\MyAnimationMod"
```

Only files inside the allowed roots can be inspected. Roots must exist, be readable directories, and cannot be symbolic links or Windows reparse points. A requested directory is rejected as an inspection target; pass a file instead.

## Configure an MCP client

Most clients have a JSON configuration with a command and argument list. Replace the executable and root paths with paths on your machine:

```json
{
  "mcpServers": {
    "bgs": {
      "command": "C:\\Tools\\BGS\\BehaviourGraphStudio.exe",
      "args": [
        "--mcp",
        "--root",
        "C:\\Modding\\MyAnimationMod"
      ]
    }
  }
}
```

For a Linux client:

```json
{
  "mcpServers": {
    "bgs": {
      "command": "/home/me/tools/bgs/BehaviourGraphStudio",
      "args": ["--mcp", "--root", "/home/me/modding/MyAnimationMod"]
    }
  }
}
```

Restart or reload the client after changing its MCP configuration. The client should discover the seven tools listed in the [MCP API reference](mcp-api).

## First successful request

Start with a small, known-good HKX file inside an allowed root. Ask the client to call:

```json
{
  "path": "C:\\Modding\\MyAnimationMod\\meshes\\actors\\character\\behaviors\\MyBehavior.hkx",
  "findingLimit": 50
}
```

That request is for `bgs.inspect_behavior`. Then use `bgs.resolve_project_chain` on the same path to see the related character, skeleton, and animation references.

If the result says `path_not_allowed`, the file is outside every `--root`. If it says `path_not_found`, check spelling and whether the file is loose on disk. Archive-only files must first be opened through BGS or extracted into a permitted workspace; the external MCP server does not grant archive or game-folder access implicitly.

## Security boundary

The root list is the trust boundary. Give BGS the smallest directories that contain the project under investigation. Do not use a drive root or a home directory unless the client genuinely needs that entire scope.

The external tool surface is intentionally limited:

| Capability | External MCP |
| --- | --- |
| Read HKX structure and metadata | Yes |
| Search bounded project data | Yes |
| Preview one clip-name change | Yes, no write |
| Apply an edit to disk | No |
| Shell or process execution | No |
| Arbitrary filesystem write | No |
| Provider credentials | No |

The in-app Assistant is a separate path. It can prepare a supported clip change, but the change remains pending until a person approves it in the BGS UI. The approval is tied to the active document revision, object, old value, and new value. Normal validation, verified save, backup, source-stamp, and undo guarantees still apply.

## Recommended workflow

1. Copy the mod file into a working directory and keep the original untouched.
2. Start BGS MCP with that working directory as the only root.
3. Inspect the behaviour and resolve its project chain.
4. Search for the object, clip, symbol, or asset involved in the problem.
5. Check the project and review every bounded finding.
6. Use the preview tool only to understand a possible clip change.
7. Open the working file in the desktop BGS editor for any edit.
8. Validate, save, reopen, inspect the `.bak`, and test in Fallout 4.

## Troubleshooting

### The client sees no tools

Confirm the configured command points to the BGS executable, includes `--mcp`, and includes at least one existing `--root`. Run the command in a terminal once and check standard error for a startup error.

### A file is rejected even though it exists

Pass the file's full path and ensure its canonical path is below an allowed root. Symlinked or reparse-point paths are rejected intentionally. Add the real project directory as a root instead of trying to bypass the check with a link.

### The preview changed nothing

That is expected. `bgs.preview_set_clip_animation` is side-effect-free. Use the desktop application and its explicit approval flow when you are ready to make a supported edit.

### Results are truncated

Inspection calls bound list sizes to keep responses predictable. Use the relevant limit parameter up to the documented cap, then narrow the query or inspect a specific object rather than requesting an unbounded dump.

Next, see [MCP API Reference](mcp-api) for every tool, parameter, and result purpose.
