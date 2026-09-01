import { fetchReleasedFrameworkHeader } from "../github.js";

export async function getHeader(): Promise<string> {
  return fetchReleasedFrameworkHeader();
}
