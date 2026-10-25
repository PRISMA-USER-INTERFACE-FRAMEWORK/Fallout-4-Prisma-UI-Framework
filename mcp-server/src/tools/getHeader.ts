import {
  fetchReleasedFrameworkHeader,
  fetchReleasedModernFrameworkHeader,
  fetchReleasedVrFrameworkHeader,
} from "../github.js";

export async function getHeader(): Promise<string> {
  return fetchReleasedFrameworkHeader();
}

export async function getModernHeader(): Promise<string> {
  return fetchReleasedModernFrameworkHeader();
}

export async function getVrHeader(): Promise<string> {
  return fetchReleasedVrFrameworkHeader();
}
