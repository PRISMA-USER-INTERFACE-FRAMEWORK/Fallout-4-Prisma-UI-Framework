export const REPO_OWNER = "PRISMA-USER-INTERFACE-FRAMEWORK";
export const REPO_NAME = "Fallout-4-Prisma-UI-Framework";
export const REPO_BRANCH = process.env.PRISMA_DOCS_BRANCH || "main";

export const FRAMEWORK_REPO = "Prisma-Matrix";
export const FRAMEWORK_VERSION = "2.2.0";
export const FRAMEWORK_RELEASE_TAG = "framework-v2.2.0";
export const FRAMEWORK_RELEASE_SOURCE_COMMIT = "1b32eb1fef28802b35cc9d43ad44152d769bebd5";
export const FRAMEWORK_API_SOURCE_COMMIT = "1b32eb1fef28802b35cc9d43ad44152d769bebd5";
export const FRAMEWORK_HEADER_PATH = "src/PrismaUI_F4_API.h";
export const FRAMEWORK_HEADER_BLOB_SHA = "02f6584829063ca59662d135c6910cc87d5bb2ee";
export const FRAMEWORK_MODERN_HEADER_PATH = "src/PrismaUI_F4_Modern_API.h";
export const FRAMEWORK_MODERN_HEADER_BLOB_SHA = "9784daab39bf66bb179e6ef12bba63c75e50d3b7";
export const FRAMEWORK_VR_HEADER_PATH = "src/PrismaUI_F4VR_API.h";
export const FRAMEWORK_VR_HEADER_BLOB_SHA = "012f810a98bfa274563c9eb8102881549ab2c9c6";
export const FRAMEWORK_PUBLIC_DOWNLOAD_URL = "https://www.nexusmods.com/fallout4/mods/105454";

export const GUIDE_FILES: Record<string, string> = {
  "2.2.0-release": "docs/2.2.0-release.md",
  "2.1.1-release": "docs/2.1.1-release.md",
  "2.1.0-release": "docs/2.1.0-release.md",
  "modern-api": "docs/modern-api.md",
  "getting-started": "docs/getting-started.md",
  "quick-start": "docs/quick-start.md",
  "examples": "docs/examples.md",
  "html-views": "docs/html-views.md",
  "modern-frameworks": "docs/modern-frameworks.md",
  "translations": "docs/translations.md",
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
  modernApiHeaderSource: string;
  modernApiHeaderBlob: string;
}
