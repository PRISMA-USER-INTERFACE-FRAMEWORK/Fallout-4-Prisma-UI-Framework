// Generates build/llms.txt for LLM scrapers following the llmstxt.org convention.
const fs = require('fs');
const path = require('path');

function extractTitle(content) {
  const m = content.match(/^#\s+(.+)$/m);
  return m ? m[1].replace(/`/g, '').trim() : null;
}

const GUIDES = [
  '2.1.0-release',
  'what-is-prismaui',
  'getting-started',
  'quick-start',
  'html-views',
  'view-lifecycle',
  'view-watchdog',
  'panel-management',
  'networking',
  'modern-frameworks',
  'examples',
  'model-preview',
  'limitations',
  'troubleshooting',
  'papyrus-bridge',
  'translations',
  'api-reference',
];

module.exports = function llmsTxtPlugin(context) {
  return {
    name: 'llms-txt-plugin',
    async postBuild({ outDir, siteConfig }) {
      const base = siteConfig.url + siteConfig.baseUrl.replace(/\/$/, '');
      const docsDir = path.join(context.siteDir, 'docs');

      const guideLines = GUIDES
        .filter((id) => fs.existsSync(path.join(docsDir, `${id}.md`)))
        .map((id) => {
          const raw = fs.readFileSync(path.join(docsDir, `${id}.md`), 'utf8');
          const title = extractTitle(raw) || id;
          return `- [${title}](${base}/docs/${id})`;
        });

      const apiLines = fs
        .readdirSync(path.join(docsDir, 'api'))
        .filter((f) => f.endsWith('.md') && f !== 'vr-extension.md')
        .sort()
        .map((f) => {
          const id = f.replace('.md', '');
          const raw = fs.readFileSync(path.join(docsDir, 'api', f), 'utf8');
          const title = extractTitle(raw) || id;
          return `- [${title}](${base}/docs/api/${id})`;
        });

      const lines = [
        '# PrismaUI F4',
        '',
        '> Current public framework: PrismaUI_F4 2.1.0.',
        '> Production web runtime: in-process Ultralight 1.4.0.',
        '> Shipping renderer: CPU BitmapSurface. legacy-runtime host/subprocess/retired runtime library are retired.',
        '> Desktop support: Fallout 4 OG 1.10.163 and AE 1.11.137+ with matching Address Library data.',
        '> Intermediate 1.10.980-1.10.984 Next-Gen runtimes are deliberately unsupported.',
        '> Public C++ API: IVPrismaUI1 through IVPrismaUI10. V10 includes capability-gated input regions.',
        '> ModelPreview bridge in 2.1.0 is `window.__prismaUI_modelPreview` API v4 and accepts NIF paths.',
        '> Required web dependencies should be bundled locally with consumer mods.',
        '> prisma-mcp is built from this public repository; do not assume an npm package is published.',
        '> In prisma-mcp, call `get_framework_release` and `get_header` before generating integration code.',
        '',
        '## Guides',
        '',
        ...guideLines,
        '',
        '## API Reference',
        '',
        `- [API Overview](${base}/docs/api-reference)`,
        ...apiLines,
        '',
        '## Historical / optional',
        '',
        `- [VR Extension](${base}/docs/api/vr-extension)`,
        `- [1.0 vs 2.0 historical migration notes](${base}/docs/1.0-vs-2.0)`,
        `- [Changelog](${base}/docs/changelog)`,
        '',
      ];

      fs.writeFileSync(path.join(outDir, 'llms.txt'), lines.join('\n'));
      console.log('[llms-txt] generated llms.txt for PrismaUI_F4 2.1.0');
    },
  };
};
