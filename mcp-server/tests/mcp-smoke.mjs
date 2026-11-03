import assert from "node:assert/strict";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StdioClientTransport } from "@modelcontextprotocol/sdk/client/stdio.js";

const transport = new StdioClientTransport({ command: process.execPath, args: ["dist/index.js"] });
const client = new Client({ name: "prisma-mcp-smoke", version: "1.0.0" });
try {
  await client.connect(transport);
  const { tools } = await client.listTools();
  assert(tools.some((tool) => tool.name === "get_framework_release"));
  const release = await client.callTool({ name: "get_framework_release", arguments: {} });
  assert(!release.isError, JSON.stringify(release));
  assert(JSON.stringify(release).includes("1.10.984"));
  const invalid = await client.callTool({ name: "get_guide", arguments: { name: "not-a-guide" } });
  assert(invalid.isError);
  console.log("Prisma MCP transport and schema checks pass");
} finally {
  await client.close();
}
