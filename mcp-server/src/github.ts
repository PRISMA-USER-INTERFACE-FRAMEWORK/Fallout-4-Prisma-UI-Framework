import { createHash } from "node:crypto";
import {
  FRAMEWORK_API_SOURCE_COMMIT,
  FRAMEWORK_HEADER_BLOB_SHA,
  FRAMEWORK_HEADER_PATH,
  FRAMEWORK_MODERN_HEADER_BLOB_SHA,
  FRAMEWORK_MODERN_HEADER_PATH,
  FRAMEWORK_PUBLIC_DOWNLOAD_URL,
  FRAMEWORK_RELEASE_SOURCE_COMMIT,
  FRAMEWORK_VR_HEADER_BLOB_SHA,
  FRAMEWORK_VR_HEADER_PATH,
  FRAMEWORK_RELEASE_TAG,
  FRAMEWORK_REPO,
  FRAMEWORK_VERSION,
  FrameworkReleaseInfo,
  GitTreeEntry,
  GitTreeResponse,
  REPO_BRANCH,
  REPO_NAME,
  REPO_OWNER,
} from "./types.js";

const CACHE_TTL_MS = 10 * 60 * 1000;
const FETCH_TIMEOUT_MS = 15 * 1000;

interface CacheEntry<T> {
  value: T;
  expiresAt: number;
}

const cache = new Map<string, CacheEntry<unknown>>();

function getCached<T>(key: string): T | undefined {
  const entry = cache.get(key);
  if (!entry) return undefined;
  if (Date.now() > entry.expiresAt) {
    cache.delete(key);
    return undefined;
  }
  return entry.value as T;
}

function setCached<T>(key: string, value: T): void {
  cache.set(key, { value, expiresAt: Date.now() + CACHE_TTL_MS });
}

function githubApiHeaders(): Record<string, string> {
  const headers: Record<string, string> = {
    Accept: "application/vnd.github+json",
    "User-Agent": "prisma-mcp",
  };
  const token = process.env.GITHUB_TOKEN;
  if (token) headers.Authorization = `Bearer ${token}`;
  return headers;
}

function rawHeaders(): Record<string, string> {
  const headers: Record<string, string> = { "User-Agent": "prisma-mcp" };
  const token = process.env.GITHUB_TOKEN;
  if (token) headers.Authorization = `Bearer ${token}`;
  return headers;
}

async function fetchText(url: string, cacheKey: string): Promise<string> {
  const cached = getCached<string>(cacheKey);
  if (cached !== undefined) return cached;

  const res = await fetch(url, {
    headers: rawHeaders(),
    signal: AbortSignal.timeout(FETCH_TIMEOUT_MS),
  });
  if (!res.ok) throw new Error(`Failed to fetch ${url}: HTTP ${res.status}`);
  const text = await res.text();
  setCached(cacheKey, text);
  return text;
}

function gitBlobSha(text: string): string {
  const body = Buffer.from(text, "utf8");
  const header = Buffer.from(`blob ${body.length}\0`, "utf8");
  return createHash("sha1").update(header).update(body).digest("hex");
}

export async function fetchRawFile(path: string): Promise<string> {
  const url = `https://raw.githubusercontent.com/${REPO_OWNER}/${REPO_NAME}/${REPO_BRANCH}/${path}`;
  return fetchText(url, `docs:${path}`);
}

async function fetchVerifiedHeader(path: string, expectedBlob: string): Promise<string> {
  const header = await fetchRawFile(path);
  const actualBlob = gitBlobSha(header);
  if (actualBlob !== expectedBlob) {
    throw new Error(`Public ${path} mirror drift: expected ${expectedBlob}, got ${actualBlob}`);
  }
  return header;
}

export async function fetchReleasedFrameworkHeader(): Promise<string> {
  return fetchVerifiedHeader(FRAMEWORK_HEADER_PATH, FRAMEWORK_HEADER_BLOB_SHA);
}

export async function fetchReleasedModernFrameworkHeader(): Promise<string> {
  return fetchVerifiedHeader(FRAMEWORK_MODERN_HEADER_PATH, FRAMEWORK_MODERN_HEADER_BLOB_SHA);
}

export async function fetchReleasedVrFrameworkHeader(): Promise<string> {
  return fetchVerifiedHeader(FRAMEWORK_VR_HEADER_PATH, FRAMEWORK_VR_HEADER_BLOB_SHA);
}

export async function fetchFrameworkReleaseInfo(): Promise<FrameworkReleaseInfo> {
  const cacheKey = `framework-release:${FRAMEWORK_RELEASE_TAG}`;
  const cached = getCached<FrameworkReleaseInfo>(cacheKey);
  if (cached !== undefined) return cached;

  await Promise.all([
    fetchReleasedFrameworkHeader(),
    fetchReleasedModernFrameworkHeader(),
    fetchReleasedVrFrameworkHeader(),
  ]);

  const info: FrameworkReleaseInfo = {
    version: FRAMEWORK_VERSION,
    tag: FRAMEWORK_RELEASE_TAG,
    sourceCommit: FRAMEWORK_RELEASE_SOURCE_COMMIT,
    apiSourceCommit: FRAMEWORK_API_SOURCE_COMMIT,
    backend: "Ultralight 1.4.0 in-process",
    renderer: "D3D11 GPU-accelerated presentation with controlled CPU BitmapSurface fallback",
    desktopRuntimes: ["Fallout 4 OG 1.10.163", "Fallout 4 AE 1.11.137+ with matching Address Library data"],
    controllerRuntimes: ["Fallout 4 OG 1.10.163", "Fallout 4 AE 1.11.240"],
    rejectedRuntimes: ["Fallout 4 1.10.980-1.10.984 intermediate Next-Gen line"],
    modernFeatures: ["Core", "Controller", "GameThread", "Meta", "View", "Interop", "Localization", "Render", "Input", "Menu"],
    releaseUrl: FRAMEWORK_PUBLIC_DOWNLOAD_URL,
    maintainerProvenanceUrl: `https://github.com/${REPO_OWNER}/${FRAMEWORK_REPO}/releases/tag/${FRAMEWORK_RELEASE_TAG}`,
    flatReleaseArtifactUrl: `https://github.com/${REPO_OWNER}/${FRAMEWORK_REPO}/releases/download/${FRAMEWORK_RELEASE_TAG}/PrismaUI_F4-${FRAMEWORK_VERSION}-Fallout4-OG-AE.zip`,
    vrReleaseArtifactUrl: `https://github.com/${REPO_OWNER}/${FRAMEWORK_REPO}/releases/download/${FRAMEWORK_RELEASE_TAG}/PrismaUI_F4-${FRAMEWORK_VERSION}-Fallout4-VR.zip`,
    apiHeaderSource: `https://github.com/${REPO_OWNER}/${REPO_NAME}/blob/${REPO_BRANCH}/${FRAMEWORK_HEADER_PATH}`,
    apiHeaderBlob: FRAMEWORK_HEADER_BLOB_SHA,
    modernApiHeaderSource: `https://github.com/${REPO_OWNER}/${REPO_NAME}/blob/${REPO_BRANCH}/${FRAMEWORK_MODERN_HEADER_PATH}`,
    modernApiHeaderBlob: FRAMEWORK_MODERN_HEADER_BLOB_SHA,
    vrApiHeaderSource: `https://github.com/${REPO_OWNER}/${REPO_NAME}/blob/${REPO_BRANCH}/${FRAMEWORK_VR_HEADER_PATH}`,
    vrApiHeaderBlob: FRAMEWORK_VR_HEADER_BLOB_SHA,
  };
  setCached(cacheKey, info);
  return info;
}

export async function fetchTree(): Promise<GitTreeEntry[]> {
  const cacheKey = "tree";
  const cached = getCached<GitTreeEntry[]>(cacheKey);
  if (cached !== undefined) return cached;

  const url = `https://api.github.com/repos/${REPO_OWNER}/${REPO_NAME}/git/trees/${REPO_BRANCH}?recursive=1`;
  const res = await fetch(url, {
    headers: githubApiHeaders(),
    signal: AbortSignal.timeout(FETCH_TIMEOUT_MS),
  });
  if (!res.ok) {
    throw new Error(
      `Failed to fetch repo tree: HTTP ${res.status}. If you're hitting GitHub's rate limit, set a GITHUB_TOKEN env var.`
    );
  }
  const data = (await res.json()) as GitTreeResponse;
  if (data.truncated) {
    throw new Error("Repo tree response was truncated by GitHub's API - this shouldn't happen for a repo this size.");
  }
  setCached(cacheKey, data.tree);
  return data.tree;
}

export async function listPathsUnder(prefix: string): Promise<string[]> {
  const tree = await fetchTree();
  return tree
    .filter((entry) => entry.type === "blob" && entry.path.startsWith(prefix))
    .map((entry) => entry.path);
}
