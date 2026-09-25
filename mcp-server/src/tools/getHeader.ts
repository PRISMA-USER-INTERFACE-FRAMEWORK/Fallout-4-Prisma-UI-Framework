import { fetchReleasedFrameworkHeader, fetchReleasedModernFrameworkHeader } from "../github.js";

export async function getHeader(): Promise<string> {
  return fetchReleasedFrameworkHeader();
}

export async function getModernHeader(): Promise<string> {
  return fetchReleasedModernFrameworkHeader();
}
