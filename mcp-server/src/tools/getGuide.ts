import { fetchRawFile } from "../github.js";
import { GUIDE_FILES } from "../types.js";

export const GUIDE_NAMES = Object.keys(GUIDE_FILES);

export async function getGuide(name: string): Promise<string> {
  const path = GUIDE_FILES[name];
  if (!path) {
    throw new Error(`Unknown guide "${name}". Valid names: ${GUIDE_NAMES.join(", ")}`);
  }
  return fetchRawFile(path);
}
