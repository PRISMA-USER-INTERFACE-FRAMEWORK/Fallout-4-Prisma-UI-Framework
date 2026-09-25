import { fetchReleasedModernFrameworkHeader } from "../github.js";

export interface ModernMethod {
  name: string;
  type: string;
  signature?: string;
}

export interface ModernFeature {
  name: string;
  id: number;
  version: number;
  table: string;
  methods: ModernMethod[];
}

function parseAliases(header: string): Map<string, string> {
  const aliases = new Map<string, string>();
  for (const match of header.matchAll(/using\s+(\w+)\s*=\s*([^;]+);/g)) {
    aliases.set(match[1], match[2].trim());
  }
  return aliases;
}

function parseFeatureIds(header: string): Map<string, number> {
  const block = header.match(/enum class ApiFeature\s*:\s*uint32_t\s*\{([\s\S]*?)\};/);
  if (!block) throw new Error("Modern API header is missing ApiFeature");
  const ids = new Map<string, number>();
  for (const match of block[1].matchAll(/(\w+)\s*=\s*(\d+)/g)) {
    ids.set(match[1], Number(match[2]));
  }
  return ids;
}

function parseVersions(header: string): Map<string, number> {
  const versions = new Map<string, number>();
  for (const match of header.matchAll(/inline constexpr uint32_t (\w+)ApiVersion\s*=\s*(\d+);/g)) {
    versions.set(match[1], Number(match[2]));
  }
  return versions;
}

export function parseModernFeatures(header: string): ModernFeature[] {
  const aliases = parseAliases(header);
  const ids = parseFeatureIds(header);
  const versions = parseVersions(header);
  const features: ModernFeature[] = [];

  for (const match of header.matchAll(/struct\s+(\w+API)\s*\{([\s\S]*?)\n\s*\};/g)) {
    const table = match[1];
    const name = table.slice(0, -3);
    if (!ids.has(name) || !versions.has(name)) continue;

    const methods: ModernMethod[] = [];
    for (const line of match[2].split("\n")) {
      const field = line.match(/^\s*(\w+)\s+(\w+);\s*$/);
      if (!field || field[2] === "structSize" || field[2] === "apiVersion") continue;
      methods.push({
        name: field[2],
        type: field[1],
        signature: aliases.get(field[1]),
      });
    }

    features.push({
      name,
      id: ids.get(name)!,
      version: versions.get(name)!,
      table,
      methods,
    });
  }

  return features.sort((a, b) => a.id - b.id);
}

export async function listModernFeatures(): Promise<ModernFeature[]> {
  return parseModernFeatures(await fetchReleasedModernFrameworkHeader());
}

export async function getModernFeature(name: string): Promise<ModernFeature> {
  const features = await listModernFeatures();
  const feature = features.find((entry) => entry.name.toLowerCase() === name.toLowerCase());
  if (feature) return feature;
  throw new Error(`Unknown modern feature "${name}". Valid features: ${features.map((entry) => entry.name).join(", ")}`);
}
