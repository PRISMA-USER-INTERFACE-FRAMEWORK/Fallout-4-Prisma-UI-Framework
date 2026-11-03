import { fetchFrameworkReleaseInfo } from "../github.js";

export async function getFrameworkRelease(): Promise<string> {
  const info = await fetchFrameworkReleaseInfo();
  return JSON.stringify(info, null, 2);
}
