# Prisma Designer Microsite Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add Prisma Designer to the Prisma website as a first-class tool with a standalone landing page, dedicated microsite navigation, and a complete integrated guide.

**Architecture:** Follow the existing Behavior Graph Studio pattern. The Tools index and detail page are React pages using the shared tools CSS, the guide is a separate Docusaurus docs plugin with its own sidebar, and `microsites.ts` selects the dedicated navbar/footer configuration by URL prefix.

**Tech Stack:** Docusaurus 3, React/TypeScript, CSS modules, Markdown docs, Node/npm.

## Global Constraints

- Keep the change limited to the `website/` Docusaurus application.
- Use `/tools/prismadesigner` for the detail page and `/tools/prismadesigner/guide` for guide pages.
- Preserve the actual Prisma Designer filenames, paths, API names, launch instructions, and live-preview limitation from the current README and user guide.
- Give the Tools card distinct badges, including `Browser Tool` and `PrismaUI F4`.
- Do not modify the Prisma Designer application or PrismaDesignerBridge repositories.
- The production build must pass with broken links treated as errors.

---

### Task 1: Add the Prisma Designer guide plugin and content

**Files:**
- Create: `website/designer-docs/getting-started.md`
- Create: `website/designer-docs/canvas-and-view-types.md`
- Create: `website/designer-docs/widgets-templates-icons-themes.md`
- Create: `website/designer-docs/properties-events-and-binding.md`
- Create: `website/designer-docs/visual-scripting.md`
- Create: `website/designer-docs/exporting-and-cpp-integration.md`
- Create: `website/designer-docs/live-game-preview.md`
- Create: `website/designer-docs/validation-project-files-and-troubleshooting.md`
- Create: `website/designerSidebars.ts`
- Modify: `website/docusaurus.config.ts`

**Interfaces:**
- Produces the route base `/tools/prismadesigner/guide` and sidebar id `designerSidebar`.
- Guide pages link to one another using relative Markdown links or stable route paths.
- The detail page and microsite config consume the guide's `getting-started` route.

- [ ] **Step 1: Extract the current Designer workflow into page outlines.**

  Use the current Prisma Designer README and `docs/designer-guide.md` as the source of truth. Keep the real setup commands, export path `Data/PrismaUI_F4/views/Interface/<YourPlugin>/`, `CreateView`, project formats, bridge limitation, and test/validation behavior.

- [ ] **Step 2: Write the eight guide pages.**

  Each page must have Docusaurus front matter with a readable title and sidebar position. The content must be task-oriented and include the concrete UI labels and code snippets users need. Cross-link the pages at the end of each workflow where it helps users continue.

- [ ] **Step 3: Define the guide sidebar.**

  Create a sidebar that lists the eight pages in workflow order:

  ```ts
  const designerSidebar = [
    'getting-started',
    'canvas-and-view-types',
    'widgets-templates-icons-themes',
    'properties-events-and-binding',
    'visual-scripting',
    'exporting-and-cpp-integration',
    'live-game-preview',
    'validation-project-files-and-troubleshooting',
  ];
  ```

- [ ] **Step 4: Register the Docusaurus docs plugin.**

  Add a second `@docusaurus/plugin-content-docs` entry after the existing BGS plugin:

  ```ts
  [
    '@docusaurus/plugin-content-docs',
    {
      id: 'designer',
      path: 'designer-docs',
      routeBasePath: 'tools/prismadesigner/guide',
      sidebarPath: './designerSidebars.ts',
    },
  ],
  ```

- [ ] **Step 5: Run Markdown and TypeScript checks.**

  Run `npm run build` from `website/` after the remaining website tasks are complete. Before that, run `git diff --check` and inspect all new guide links for typos.

### Task 2: Add the Prisma Designer detail page and microsite navigation

**Files:**
- Create: `website/src/pages/tools/prismadesigner.tsx`
- Modify: `website/src/microsites.ts`

**Interfaces:**
- Produces the `/tools/prismadesigner` page.
- `MICROSITES` gains a `pathPrefix` of `/tools/prismadesigner` and links the guide, Tools index, Prisma Designer GitHub repository, and GitHub releases.

- [ ] **Step 1: Build the detail page from the shared Behavior Graph Studio layout.**

  Use `Layout`, `Link`, `useBaseUrl`, and `./styles.module.css`. Include badges for `Browser Tool`, `PrismaUI F4`, and `No install`. Add a primary GitHub/release CTA and a secondary guide CTA.

- [ ] **Step 2: Add concrete overview sections.**

  Include sections for what Prisma Designer is, the design-to-export workflow, visual scripting and binding, and live game preview. State that the bridge preview covers render/layout only and that real C++ listeners are verified from the exported view in the user's plugin.

- [ ] **Step 3: Add the screenshot showcase and guide cards.**

  Use `/img/prisma-designer-main-menu.png` for the main image and add guide cards for all eight guide routes. End with a `/tools` back link.

- [ ] **Step 4: Register the microsite entry.**

  Add internal links for Guide and All Tools, external links for GitHub and Releases, and a `copyrightName` of `Prisma Designer`. Use `https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Prisma-Designer` as the repository URL.

### Task 3: Add the Tools card, screenshot asset, and tool-specific badges

**Files:**
- Modify: `website/src/pages/tools/index.tsx`
- Create: `website/static/img/prisma-designer-main-menu.png`

**Interfaces:**
- The Tools index lists Prisma Designer at `/tools/prismadesigner`.
- The card has its own badge set and uses the screenshot asset without changing shared card CSS.

- [ ] **Step 1: Copy the existing release screenshot into the website.**

  Copy `PrismaDesigner/downloads/screenshots/01-main-menu.png` from the local Prisma Designer checkout to `website/static/img/prisma-designer-main-menu.png`. Preserve the image bytes and use it for both the card and detail page.

- [ ] **Step 2: Add the card entry.**

  Add a `prismadesigner` item to `TOOLS` with tags `Browser Tool` and `PrismaUI F4`, the pitch `A no-install visual editor for building HTML/CSS/JavaScript views for PrismaUI F4, with export, binding, visual scripting, and optional live game preview.`, the detail route, and `imageFit: 'cover'`.

- [ ] **Step 3: Confirm card rendering inputs.**

  Check that the image path is passed through `useBaseUrl`, the card key is unique, and the card points to the new route rather than an external URL.

### Task 4: Verify the complete website

**Files:**
- Modify only files required to correct build or broken-link failures discovered during verification.

**Interfaces:**
- Final branch contains the spec commit plus implementation commits with a clean working tree.

- [ ] **Step 1: Install website dependencies if needed.**

  From `website/`, run `npm ci` when `node_modules` is absent or does not match `package-lock.json`.

- [ ] **Step 2: Run static checks.**

  Run `git diff --check` from the repository root and inspect the changed-file list. Confirm all guide files have front matter and all internal links use Docusaurus routes.

- [ ] **Step 3: Build the production website.**

  Run `npm run build` from `website/`. Expected result: successful Docusaurus build with no broken-link errors.

- [ ] **Step 4: Inspect generated routes.**

  Confirm the build output contains the Tools page, Prisma Designer detail page, and all eight guide pages under the configured base URL. Confirm the microsite path prefix covers the detail page and guide routes.

- [ ] **Step 5: Commit implementation.**

  Stage only the website implementation files and commit with:

  ```bash
  git add website/docusaurus.config.ts website/designerSidebars.ts website/designer-docs website/src/microsites.ts website/src/pages/tools/index.tsx website/src/pages/tools/prismadesigner.tsx website/static/img/prisma-designer-main-menu.png
  git commit -m "feat(website): add Prisma Designer tool"
  ```
