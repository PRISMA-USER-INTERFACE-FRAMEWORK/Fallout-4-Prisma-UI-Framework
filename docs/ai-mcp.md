# AI & MCP integration

PrismaUI's documentation website is designed to work well with both MCP-capable coding agents and ordinary AI chat tools.

Every PrismaUI documentation page provides two quick actions:

- **Copy as Markdown** copies the exact raw Markdown source for the current page.
- **Ask AI** opens the AI/MCP panel. From there you can connect Prisma MCP, or copy the current page and open ChatGPT, Claude, or Gemini for a quick question.

For real integration work, use **Prisma MCP**. It gives the coding agent structured access to the public PrismaUI developer surface instead of relying on copied snippets or stale training data.

## What Prisma MCP provides

The public `mcp-server` exposes these tools:

- `get_framework_release` - current released framework identity, backend, renderer, runtime support, and public SDK provenance.
- `get_header` - verified released desktop API header.
- `list_api_methods` - documented public API methods and interface versions.
- `get_api_method` - full documentation for one API method.
- `search_docs` - search the current API reference and developer guides.
- `get_guide` - fetch a complete current developer guide.
- `scaffold_plugin` - create the official example consumer structure at a user-confirmed local path.

The public repository is the source for documentation and SDK mirrors. Users do not need access to the private Prisma-Matrix implementation repository to use the MCP documentation tools.

## Requirements

- Node.js 20 or newer
- Git
- an MCP-compatible client or coding agent

## Build Prisma MCP

```bash
git clone https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Fallout-4-Prisma-UI-Framework.git
cd Fallout-4-Prisma-UI-Framework/mcp-server
npm ci
npm run build
```

The stdio server entry point is:

```text
/FULL/PATH/Fallout-4-Prisma-UI-Framework/mcp-server/dist/index.js
```

## Generic MCP client configuration

Use your actual absolute path:

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

Exact configuration placement varies by MCP client, but the transport is ordinary stdio and the command should launch the built `dist/index.js` with Node.

## Recommended agent workflow

For PrismaUI development, instruct the agent to:

1. Call `get_framework_release` when framework version, runtime support, or renderer behavior matters.
2. Call `get_header` before generating or reviewing native C++ integration code.
3. Use `search_docs` or `get_guide` for lifecycle, controller, networking, panel, ModelPreview, and troubleshooting questions.
4. Use `get_api_method` for exact signatures and lifecycle/threading requirements.
5. Request the lowest API interface that actually contains the features the consumer needs.
6. Never infer old browser-runtime architecture, unsupported Fallout runtime support, or undocumented APIs from older examples.

## Controller API work

For controller integration, the agent should read the current [Controller Actions guide](controller-actions.md) and the [API Reference](api-reference.md) before changing a consumer.

The V12 controller surface is focused-view owned. Bindings are registered after DOM-ready, actions arrive as `prisma-controller-action`, and mapped events are consumed only for the exact focused live view.

## Copy as Markdown

The website publishes raw `.md` alongside each built documentation page. The **Copy as Markdown** button fetches that exact source and writes it to the clipboard.

This is useful when:

- the AI tool does not support MCP;
- you want to ask about one page only;
- you want to paste exact documentation into an issue, PR, or local note.

## Ask AI

**Ask AI** is the lightweight fallback. Choose an AI provider, and the site will try to copy the current page as Markdown before opening that provider. Paste the Markdown into the new chat.

This does not replace MCP. MCP is preferred for coding work because the agent can query multiple current guides and API method pages directly instead of seeing only one copied page.

## See also

- [API Reference](api-reference.md)
- [Controller Actions](controller-actions.md)
- [Getting Started](getting-started.md)
- [Troubleshooting](troubleshooting.md)
