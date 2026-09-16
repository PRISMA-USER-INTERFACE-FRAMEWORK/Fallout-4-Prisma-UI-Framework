import { fetchFrameworkReleaseInfo, fetchReleasedFrameworkHeader } from "./github.js";
import {
  FRAMEWORK_API_SOURCE_COMMIT,
  FRAMEWORK_HEADER_BLOB_SHA,
  FRAMEWORK_RELEASE_SOURCE_COMMIT,
  FRAMEWORK_VERSION,
} from "./types.js";

async function main(): Promise<void> {
  const header = await fetchReleasedFrameworkHeader();
  if (!header.includes("class IVPrismaUI12")) {
    throw new Error("Verified SDK mirror does not contain IVPrismaUI12");
  }
  if (!header.includes("enum class InterfaceVersion") || !header.includes("V11, V12")) {
    throw new Error("Verified SDK mirror does not advertise V11/V12");
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

  console.log(
    `verified PrismaUI_F4 ${release.version} V1-V12 SDK ${release.apiHeaderBlob} from ${release.apiHeaderSource}`
  );
}

main().catch((error: unknown) => {
  console.error(error instanceof Error ? error.message : String(error));
  process.exitCode = 1;
});
