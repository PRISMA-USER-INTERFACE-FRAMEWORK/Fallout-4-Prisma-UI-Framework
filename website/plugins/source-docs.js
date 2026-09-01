// places raw .md files alongside the built HTML so any doc page URL + ".md"
// returns the Markdown source. e.g. /Fallout-4-Prisma-UI-Framework/docs/getting-started.md
//
// only .md files are copied; the existing HTML directory structure is untouched.
const fs = require('fs');
const path = require('path');

function copyMd(srcDir, destDir) {
  for (const entry of fs.readdirSync(srcDir, { withFileTypes: true })) {
    const srcPath = path.join(srcDir, entry.name);
    const destPath = path.join(destDir, entry.name);
    if (entry.isDirectory()) {
      fs.mkdirSync(destPath, { recursive: true });
      copyMd(srcPath, destPath);
    } else if (entry.name.endsWith('.md')) {
      fs.copyFileSync(srcPath, destPath);
    }
  }
}

module.exports = function sourceDocsPlugin(context) {
  return {
    name: 'source-docs-plugin',
    async postBuild({ outDir }) {
      const srcDir = path.join(context.siteDir, 'docs');
      const destDir = path.join(outDir, 'docs');
      copyMd(srcDir, destDir);
    },
  };
};
