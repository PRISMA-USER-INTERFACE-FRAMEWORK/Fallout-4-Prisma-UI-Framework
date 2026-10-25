// copies ../docs/ and ../CHANGELOG.md into website/docs/ before every build.
// website/docs/ is gitignored — the repo root docs/ is the single source of truth.
//
// also rewrites a small set of repo-relative links that point outside the docs/
// tree (../README.md, ../mcp-server/) into their equivalent GitHub URLs, since
// Docusaurus can't resolve them during the broken-links check.
import { cpSync, copyFileSync, rmSync, mkdirSync, existsSync, readFileSync, writeFileSync } from 'fs';
import { join, relative } from 'path';
import { fileURLToPath } from 'url';
import { readdirSync, statSync } from 'fs';

const scriptDir = fileURLToPath(new URL('.', import.meta.url));
const repoRoot = join(scriptDir, '..', '..');
const docsTarget = join(scriptDir, '..', 'docs');

const GH = 'https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Fallout-4-Prisma-UI-Framework';

// link patterns that are valid on GitHub but break Docusaurus's checker
const LINK_REWRITES = [
  // directory link to ../mcp-server/ → GitHub tree link
  [/\]\(\.\.\/mcp-server\/\)/g, `](${GH}/tree/main/mcp-server)`],
  // section links into ../README.md → GitHub blob link
  [/\]\(\.\.\/README\.md(#[^)]+)?\)/g, (_, hash) => `](${GH}/blob/main/README.md${hash ?? ''})`],
  // bare directory link api/ in api-reference.md → first method page
  [/\]\(api\/\)/g, '](api/CreateView)'],
];

function rewriteLinks(content) {
  let out = content;
  for (const [pattern, replacement] of LINK_REWRITES) {
    out = out.replace(pattern, replacement);
  }
  return out;
}

function walkFiles(dir) {
  const results = [];
  for (const name of readdirSync(dir)) {
    const full = join(dir, name);
    if (statSync(full).isDirectory()) {
      results.push(...walkFiles(full));
    } else {
      results.push(full);
    }
  }
  return results;
}

// clear and repopulate docs from repo root
if (existsSync(docsTarget)) {
  rmSync(docsTarget, { recursive: true, force: true });
}
mkdirSync(docsTarget, { recursive: true });

cpSync(join(repoRoot, 'docs'), docsTarget, { recursive: true });
copyFileSync(join(repoRoot, 'CHANGELOG.md'), join(docsTarget, 'changelog.md'));

// rewrite cross-repo links in every copied markdown file
for (const file of walkFiles(docsTarget)) {
  if (!file.endsWith('.md') && !file.endsWith('.mdx')) continue;
  const original = readFileSync(file, 'utf8');
  const rewritten = rewriteLinks(original);
  if (rewritten !== original) {
    writeFileSync(file, rewritten);
  }
}

console.log('synced docs/ and CHANGELOG.md into website/docs/');
