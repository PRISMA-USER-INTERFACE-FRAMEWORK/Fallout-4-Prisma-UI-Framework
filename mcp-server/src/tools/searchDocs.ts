import { fetchRawFile } from "../github.js";
import { GUIDE_FILES } from "../types.js";
import { listApiMethodNames } from "./getApiMethod.js";

export interface SearchResult {
  source: string;
  path: string;
  snippet: string;
}

const SNIPPET_RADIUS = 80;

function findSnippet(content: string, query: string): string | undefined {
  const idx = content.toLowerCase().indexOf(query.toLowerCase());
  if (idx === -1) return undefined;
  const start = Math.max(0, idx - SNIPPET_RADIUS);
  const end = Math.min(content.length, idx + query.length + SNIPPET_RADIUS);
  const prefix = start > 0 ? "..." : "";
  const suffix = end < content.length ? "..." : "";
  return (prefix + content.slice(start, end).replace(/\s+/g, " ").trim() + suffix);
}

export async function searchDocs(query: string): Promise<SearchResult[]> {
  const results: SearchResult[] = [];

  const apiNames = await listApiMethodNames();
  const apiPaths = apiNames.map((name) => ({ source: `api/${name}`, path: `docs/api/${name}.md` }));
  const guidePaths = Object.entries(GUIDE_FILES).map(([source, path]) => ({ source, path }));

  const allTargets = [...apiPaths, ...guidePaths];

  const contents = await Promise.all(
    allTargets.map(async (t) => {
      try {
        return { ...t, content: await fetchRawFile(t.path) };
      } catch {
        return { ...t, content: "" };
      }
    })
  );

  for (const target of contents) {
    const snippet = findSnippet(target.content, query);
    if (snippet) {
      results.push({ source: target.source, path: target.path, snippet });
    }
  }

  return results;
}
