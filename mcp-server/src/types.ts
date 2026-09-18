export const REPO_OWNER = "PRISMA-USER-INTERFACE-FRAMEWORK";
export const REPO_NAME = "Fallout-4-Prisma-UI-Framework";
export const REPO_BRANCH = process.env.PRISMA_DOCS_BRANCH || "main";

export const FRAMEWORK_REPO = "Prisma-Matrix";
export const FRAMEWORK_VERSION = "2.1.0";
export const FRAMEWORK_RELEASE_TAG = "framework-v2.1.0";
export const FRAMEWORK_RELEASE_SOURCE_COMMIT = "061f699864500cd754c9aac854eb047093a161ea";
export const FRAMEWORK_API_SOURCE_COMMIT = "61848ed3fa65f41f4fb0f2d169f995a41951f62f";
export const FRAMEWORK_HEADER_PATH = "src/PrismaUI_F4_API.h";
export const FRAMEWORK_HEADER_BLOB_SHA = "23755e874f311a4294e662d3f6dc332547de5142";
export const FRAMEWORK_VR_HEADER_PATH = "src/PrismaUI_F4VR_API.h";
export const FRAMEWORK_VR_HEADER_BLOB_SHA = "8221eb7bd81694f604b6f188fc8b2c475200dbf0";
export const FRAMEWORK_PUBLIC_DOWNLOAD_URL = "https://www.nexusmods.com/fallout4/mods/105454";

export const GUIDE_FILES: Record<string, string> = {
  "2.1.0-release": "docs/2.1.0-release.md",
  "getting-started": "docs/getting-started.md",
  "quick-start": "docs/quick-start.md",
  "examples": "docs/examples.md",
  "html-views": "docs/html-views.md",
  "modern-frameworks": "docs/modern-frameworks.md",
  "modern-api": "docs/modern-api.md",
  "networking": "docs/networking.md",
  "view-lifecycle": "docs/view-lifecycle.md",
  "view-watchdog": "docs/view-watchdog.md",
  "panel-management": "docs/panel-management.md",
  "vanilla-ui-suppression": "docs/vanilla-ui-suppression.md",
  "model-preview": "docs/model-preview.md",
  "papyrus-bridge": "docs/papyrus-bridge.md",
  "controller-actions": "docs/controller-actions.md",
  "api-extensions": "docs/api-extensions.md",
  "troubleshooting": "docs/troubleshooting.md",
  "limitations": "docs/limitations.md",
  "api-reference": "docs/api-reference.md",
};

export interface GitTreeEntry {
  path: string;
  mode: string;
  type: "blob" | "tree" | "commit";
  sha: string;
  size?: number;
  url: string;
}

export interface GitTreeResponse {
  sha: string;
  tree: GitTreeEntry[];
  truncated: boolean;
}

export interface FrameworkReleaseInfo {
  version: string;
  tag: string;
  sourceCommit: string;
  apiSourceCommit: string;
  backend: string;
  renderer: string;
  desktopRuntimes: string[];
  rejectedRuntimes: string[];
  releaseUrl: string;
  maintainerProvenanceUrl: string;
  apiHeaderSource: string;
  apiHeaderBlob: string;
}
