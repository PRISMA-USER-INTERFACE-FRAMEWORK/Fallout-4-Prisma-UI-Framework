import { extractSinceVersion, extractSummary, getApiMethodDoc, listApiMethodNames } from "./getApiMethod.js";

export interface ApiMethodSummary {
  name: string;
  since?: string;
  summary: string;
}

export async function listApiMethods(sinceVersion?: string): Promise<ApiMethodSummary[]> {
  const names = await listApiMethodNames();
  const docs = await Promise.all(names.map((name) => getApiMethodDoc(name)));

  const entries: ApiMethodSummary[] = docs.map((doc) => ({
    name: doc.name,
    since: extractSinceVersion(doc.content),
    summary: extractSummary(doc.content),
  }));

  if (!sinceVersion) return entries;

  const normalized = sinceVersion.toUpperCase().startsWith("V") ? sinceVersion.toUpperCase() : `V${sinceVersion}`;
  return entries.filter((e) => e.since === normalized);
}
