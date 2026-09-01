import { mkdir, readdir, stat, writeFile } from "node:fs/promises";
import { dirname, join, relative, resolve } from "node:path";
import { fetchRawFile, listPathsUnder } from "../github.js";
import { getHeader } from "./getHeader.js";

const EXAMPLE_PREFIX = "example-f4se-plugin/";
const EXAMPLE_TOKEN = "PrismaUI-F4-Example";
const API_HEADER_RELATIVE_PATH = "src/PrismaUI_F4_API.h";

const PLUGIN_NAME_PATTERN = /^[A-Za-z][A-Za-z0-9_-]*$/;

export interface ScaffoldResult {
  targetPath: string;
  filesWritten: string[];
}

async function assertWritableTarget(absTarget: string, overwrite: boolean): Promise<void> {
  let stats;
  try {
    stats = await stat(absTarget);
  } catch {
    return;
  }

  if (!stats.isDirectory()) {
    throw new Error(`"${absTarget}" already exists and is not a directory.`);
  }

  if (overwrite) return;

  const entries = await readdir(absTarget);
  if (entries.length > 0) {
    throw new Error(
      `"${absTarget}" already exists and is not empty. Pass overwrite=true if you really want to write into it.`
    );
  }
}

export async function scaffoldPlugin(
  pluginName: string,
  targetPath: string,
  overwrite = false
): Promise<ScaffoldResult> {
  if (!PLUGIN_NAME_PATTERN.test(pluginName)) {
    throw new Error(
      `Invalid plugin name "${pluginName}". Use letters, digits, hyphens, and underscores, starting with a letter (e.g. "MyPlugin_F4").`
    );
  }

  const absTarget = resolve(targetPath);
  await assertWritableTarget(absTarget, overwrite);

  const sourcePaths = await listPathsUnder(EXAMPLE_PREFIX);
  if (sourcePaths.length === 0) {
    throw new Error(`Found no files under "${EXAMPLE_PREFIX}" in the repo - this shouldn't happen.`);
  }

  // Fetch the authoritative public 2.1.0 header once. The example copy is useful for humans browsing
  // the repo, but generated projects must not trust a duplicate that could drift between releases.
  const releasedHeader = await getHeader();

  // Fetch everything before writing, so a mid-fetch failure never leaves a half-written project.
  const files = await Promise.all(
    sourcePaths.map(async (sourcePath) => {
      const relativePath = sourcePath.slice(EXAMPLE_PREFIX.length);
      const destPath = resolve(absTarget, relativePath);
      if (relative(absTarget, destPath).startsWith("..")) {
        throw new Error(`Refusing to write outside the target directory: ${relativePath}`);
      }

      const rawContent =
        relativePath === API_HEADER_RELATIVE_PATH ? releasedHeader : await fetchRawFile(sourcePath);
      const renamedContent = rawContent.split(EXAMPLE_TOKEN).join(pluginName);
      return { relativePath, destPath, content: renamedContent };
    })
  );

  const filesWritten: string[] = [];
  for (const file of files) {
    await mkdir(dirname(file.destPath), { recursive: true });
    await writeFile(file.destPath, file.content, "utf8");
    filesWritten.push(file.relativePath);
  }

  return { targetPath: absTarget, filesWritten };
}
