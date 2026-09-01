#!/usr/bin/env node
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { z } from "zod";
import { getApiMethodDoc } from "./tools/getApiMethod.js";
import { getFrameworkRelease } from "./tools/getFrameworkRelease.js";
import { getGuide, GUIDE_NAMES } from "./tools/getGuide.js";
import { getHeader } from "./tools/getHeader.js";
import { listApiMethods } from "./tools/listApiMethods.js";
import { scaffoldPlugin } from "./tools/scaffoldPlugin.js";
import { searchDocs } from "./tools/searchDocs.js";

const server = new McpServer({
  name: "prisma-mcp",
  version: "1.0.0",
});

function textResult(text: string) {
  return { content: [{ type: "text" as const, text }] };
}

function errorResult(message: string) {
  return { content: [{ type: "text" as const, text: message }], isError: true };
}

server.tool(
  "get_framework_release",
  "Get the pinned PrismaUI_F4 2.1.0 release and current SDK contract targeted by this MCP server: version, " +
    "release tag, original release source commit, current API source commit, rendering backend, supported desktop Fallout " +
    "runtimes, rejected runtime line, public download URL, maintainer provenance URL, and verified V1-V12 " +
    "API-header mirror. Call this before generating setup or compatibility guidance.",
  {},
  async () => {
    try {
      return textResult(await getFrameworkRelease());
    } catch (err) {
      return errorResult(err instanceof Error ? err.message : String(err));
    }
  }
);

server.tool(
  "list_api_methods",
  "List every documented public PrismaUI_F4 API method (name, interface version, one-line summary). " +
    "Optionally filter to methods added in a specific interface version (e.g. \"V12\").",
  { sinceVersion: z.string().optional().describe('Interface version filter, e.g. "V12" or "12".') },
  async ({ sinceVersion }) => {
    try {
      const entries = await listApiMethods(sinceVersion);
      return textResult(JSON.stringify(entries, null, 2));
    } catch (err) {
      return errorResult(err instanceof Error ? err.message : String(err));
    }
  }
);

server.tool(
  "get_api_method",
  "Get the full documentation for one PrismaUI_F4 API method by name (for example CreateView, " +
    "BindViewToGeometry, DispatchToGameThread, or BindControllerAction): signature, parameters, return value, " +
    "threading notes, call-order requirements, and a usage example.",
  { name: z.string().describe('Exact method name, e.g. "CreateView".') },
  async ({ name }) => {
    try {
      const doc = await getApiMethodDoc(name);
      return textResult(doc.content);
    } catch (err) {
      return errorResult(err instanceof Error ? err.message : String(err));
    }
  }
);

server.tool(
  "search_docs",
  "Keyword search across every API method doc and guide. Use this when you do not know the exact " +
    "method name, or when checking runtime/backend guidance such as Ultralight, AE, networking, " +
    "input regions, controller actions, or panel focus.",
  { query: z.string().describe("Search term or short phrase.") },
  async ({ query }) => {
    try {
      const results = await searchDocs(query);
      if (results.length === 0) return textResult(`No matches for "${query}".`);
      return textResult(JSON.stringify(results, null, 2));
    } catch (err) {
      return errorResult(err instanceof Error ? err.message : String(err));
    }
  }
);

server.tool(
  "get_guide",
  `Get the full text of one PrismaUI_F4 guide: ${GUIDE_NAMES.join(", ")}.`,
  { name: z.enum(GUIDE_NAMES as [string, ...string[]]).describe("Which guide to fetch.") },
  async ({ name }) => {
    try {
      return textResult(await getGuide(name));
    } catch (err) {
      return errorResult(err instanceof Error ? err.message : String(err));
    }
  }
);

server.tool(
  "get_header",
  "Get the public Fallout-4-Prisma-UI-Framework mirror of the canonical PrismaUI_F4 desktop API header. " +
    "The server recomputes its Git blob SHA and fails closed unless the bytes match the pinned V1-V12 SDK header. " +
    "Use it before writing or checking C++ signatures, parameter order, defaults, capabilities, or interface versions. " +
    "The SDK uses one PrismaUI_F4_API.h file for V1 through V12. No access to the private implementation repository is required.",
  {},
  async () => {
    try {
      return textResult(await getHeader());
    } catch (err) {
      return errorResult(err instanceof Error ? err.message : String(err));
    }
  }
);

server.tool(
  "scaffold_plugin",
  "Create a new F4SE consumer project at a local path by copying and renaming PrismaUI_F4's " +
    "example plugin. Writes real files under targetPath; call this only after the user confirms the " +
    "target path and plugin name. The scaffold replaces its desktop API header with the verified canonical " +
    "V1-V12 public mirror before writing the project.",
  {
    pluginName: z
      .string()
      .describe('Plugin name, e.g. "MyPlugin_F4". Letters/digits/hyphens/underscores, starting with a letter.'),
    targetPath: z.string().describe("Local filesystem path to create the project at."),
    overwrite: z
      .boolean()
      .optional()
      .describe("Write into targetPath even if it already exists and is non-empty. Default false."),
  },
  async ({ pluginName, targetPath, overwrite }) => {
    try {
      const result = await scaffoldPlugin(pluginName, targetPath, overwrite ?? false);
      return textResult(
        `Scaffolded "${pluginName}" at ${result.targetPath}\n\n` +
          `Files written (${result.filesWritten.length}):\n` +
          result.filesWritten.map((f) => `  ${f}`).join("\n")
      );
    } catch (err) {
      return errorResult(err instanceof Error ? err.message : String(err));
    }
  }
);

async function main() {
  const transport = new StdioServerTransport();
  await server.connect(transport);
}

main().catch((err) => {
  console.error("prisma-mcp failed to start:", err);
  process.exit(1);
});
