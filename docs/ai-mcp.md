# AI and MCP

The documentation is built to be read by an assistant as well as by you. Every page can be copied as raw Markdown, and the repository ships a small MCP server that lets a coding agent look up the current API instead of guessing from training data.

## The two buttons on every page

- **Copy as Markdown** puts the exact source of the current page on your clipboard.
- **Ask AI** copies the page and opens ChatGPT, Claude or Gemini for a quick question about it.

Both are useful for a one-off question. For actual integration work, use MCP.

## Why MCP instead of copy/paste

A pasted page gives the assistant one page. Prisma MCP lets it ask for the right page itself — the current header, the exact method signature, the guide that covers the feature — so it does not blend in stale examples from older PrismaUI versions.

The public `mcp-server` exposes:

| Tool | What it gives the agent |
| --- | --- |
| `get_framework_release` | Released framework identity, backend, renderer, runtime support and SDK provenance. |
| `get_header` | The current released desktop API header. |
| `list_api_methods` | Public API methods and the interface versions they belong to. |
| `get_api_method` | Full documentation for one method: signature, parameters, returns, timing. |
| `search_docs` | Search across the current guides and API reference. |
| `get_guide` | Fetch a complete guide. |
| `scaffold_plugin` | Create the official example plugin structure at a path you confirm. |

The public repository is the source for the docs and SDK mirrors. You do not need access to the private implementation repository to use it.

## Requirements

- Node.js 20 or newer
- Git
- an MCP-capable client (or any agent that can run a stdio MCP server)

## Build it

```bash
git clone https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Fallout-4-Prisma-UI-Framework.git
cd Fallout-4-Prisma-UI-Framework/mcp-server
npm ci
npm run build
```

The server entry point is:

```text
/FULL/PATH/Fallout-4-Prisma-UI-Framework/mcp-server/dist/index.js
```

## Connect your client

```json
{
  "mcpServers": {
    "prisma-mcp": {
      "command": "node",
      "args": [
        "/FULL/PATH/Fallout-4-Prisma-UI-Framework/mcp-server/dist/index.js"
      ]
    }
  }
}
```

Use your real absolute path. The transport is ordinary stdio; how you register the server differs between clients, but the command is always "run `dist/index.js` with Node".

## How to ask the agent to work

A short instruction at the start of a session goes a long way:

```text
For PrismaUI work:
1. Call get_framework_release when version, runtime support or renderer behavior matters.
2. Call get_header before writing native C++.
3. Use search_docs or get_guide for lifecycle, controller, networking, panel, ModelPreview and troubleshooting questions.
4. Use get_api_method for exact signatures and threading rules.
5. Request the lowest interface version that actually has the feature.
6. Do not assume old browser-runtime architecture or undocumented calls from old examples.
```

With that, the assistant starts from the current contract instead of from a memory of a previous version.

## Controller work

Controller actions are the part most likely to be described out of date by an assistant. Point it at:

- [Controller actions](controller-actions) — the current event contract;
- [API by feature: Controller](api-groups/controller) — the method list;
- [`BindControllerAction`](api/BindControllerAction) — the exact call.

The short version: bind after DOM-ready, actions arrive as `prisma-controller-action` on the focused view, and unmapped D-pad/A/B keep normal navigation.

## Copy as Markdown

The site publishes the raw `.md` next to every built page. Copy as Markdown fetches that source, so what you paste is the documentation — not a re-rendering of it.

Use it when:

- your assistant cannot run MCP;
- you only need one page;
- you want the exact source for an issue or a PR.

## Ask AI

A lightweight fallback: choose a provider, the site copies the current page, and you paste it into the new chat.

It does not know about other pages. For coding work, MCP is the better answer because the agent can follow the topic.

## Where to go next

- [API reference](api-reference) — every method page
- [API by feature](/docs/api-groups/) — the modern tables
- [Your first panel](quick-start) — a known-good starting project
- [The mcp-server source](https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Fallout-4-Prisma-UI-Framework/tree/main/mcp-server)
