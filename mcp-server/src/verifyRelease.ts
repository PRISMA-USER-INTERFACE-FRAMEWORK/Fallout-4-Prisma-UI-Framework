import {
  fetchFrameworkReleaseInfo,
  fetchReleasedFrameworkHeader,
  fetchReleasedModernFrameworkHeader,
  fetchReleasedVrFrameworkHeader,
} from "./github.js";
import { getGuide } from "./tools/getGuide.js";
import { parseModernFeatures } from "./tools/modernApi.js";
import {
  FRAMEWORK_API_SOURCE_COMMIT,
  FRAMEWORK_HEADER_BLOB_SHA,
  FRAMEWORK_RELEASE_SOURCE_COMMIT,
  FRAMEWORK_VERSION,
  FRAMEWORK_VR_HEADER_BLOB_SHA,
} from "./types.js";

async function main(): Promise<void> {
  const header = await fetchReleasedFrameworkHeader();
  if (!header.includes("class IVPrismaUI12")) {
    throw new Error("Verified SDK mirror does not contain IVPrismaUI12");
  }
  if (!header.includes("enum class InterfaceVersion") || !header.includes("V11 = 138") || !header.includes("V12 = 139")) {
    throw new Error("Verified SDK mirror does not advertise V11/V12");
  }

  const modernHeader = await fetchReleasedModernFrameworkHeader();
  const modernFeatures = parseModernFeatures(modernHeader);
  const requiredFeatures = ["Core", "Controller", "GameThread", "Meta", "View", "Interop", "Localization", "Render", "Input", "Menu"];
  if (requiredFeatures.some((name) => !modernFeatures.some((feature) => feature.name === name))) {
    throw new Error("Verified modern SDK mirror does not expose all released feature tables");
  }
  const controller = modernFeatures.find((feature) => feature.name === "Controller");
  const localization = modernFeatures.find((feature) => feature.name === "Localization");
  if (!controller?.methods.some((method) => method.name === "SetNativeGamepad") ||
      !controller.methods.some((method) => method.name === "GetControllerActionBridgeState") ||
      !localization?.methods.some((method) => method.name === "RegisterTranslationsV4")) {
    throw new Error("Verified modern SDK mirror is missing released controller or localization methods");
  }

  const vrHeader = await fetchReleasedVrFrameworkHeader();
  if (!vrHeader.includes("class IVPrismaUIVR1") ||
      !vrHeader.includes("SubmitSpatialPointerUpdate") ||
      !vrHeader.includes("GetSpatialCapabilities")) {
    throw new Error("Verified VR SDK mirror is missing released spatial API contracts");
  }

  const translationsGuide = await getGuide("translations");
  if (!translationsGuide.includes("RegisterTranslationsV4")) {
    throw new Error("Translations guide is missing from the MCP guide catalog or lacks V4 localization");
  }

  const release211Guide = await getGuide("2.1.1-release");
  if (!release211Guide.includes("1.11.240") || !release211Guide.includes("Prisma Dock compatibility")) {
    throw new Error("PrismaUI 2.1.1 guide is missing release-scope controller or Dock coverage");
  }

  const release = await fetchFrameworkReleaseInfo();
  if (release.version !== FRAMEWORK_VERSION) {
    throw new Error(`Framework version mismatch: ${release.version}`);
  }
  if (release.sourceCommit !== FRAMEWORK_RELEASE_SOURCE_COMMIT) {
    throw new Error(`Framework release source mismatch: ${release.sourceCommit}`);
  }
  if (release.apiSourceCommit !== FRAMEWORK_API_SOURCE_COMMIT) {
    throw new Error(`Framework API source mismatch: ${release.apiSourceCommit}`);
  }
  if (release.apiHeaderBlob !== FRAMEWORK_HEADER_BLOB_SHA) {
    throw new Error(`Framework SDK blob mismatch: ${release.apiHeaderBlob}`);
  }
  if (release.vrApiHeaderBlob !== FRAMEWORK_VR_HEADER_BLOB_SHA) {
    throw new Error(`Framework VR SDK blob mismatch: ${release.vrApiHeaderBlob}`);
  }
  if (!release.controllerRuntimes.includes("Fallout 4 AE 1.11.240") ||
      !release.modernFeatures.includes("Localization")) {
    throw new Error("Framework release metadata is missing current controller or modern-feature boundaries");
  }

  console.log(
    `verified PrismaUI_F4 ${release.version} V1-V12 SDK ${release.apiHeaderBlob} from ${release.apiHeaderSource}`
  );
}

main().catch((error: unknown) => {
  console.error(error instanceof Error ? error.message : String(error));
  process.exitCode = 1;
});
