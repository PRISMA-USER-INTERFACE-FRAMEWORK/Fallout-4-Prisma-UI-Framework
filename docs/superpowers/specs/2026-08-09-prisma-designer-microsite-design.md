# Prisma Designer Microsite Design

## Goal

Add Prisma Designer to the Prisma website as a first-class tool, with the same
standalone microsite treatment used by Behavior Graph Studio and a complete,
task-oriented guide for building PrismaUI F4 views.

## Scope

The change is limited to the `website/` Docusaurus application:

- Add a Prisma Designer card to the Tools index, including its own badges.
- Add a detail page at `/tools/prismadesigner`.
- Add a dedicated guide at `/tools/prismadesigner/guide`.
- Add Prisma Designer microsite navigation and footer links.
- Add one existing Prisma Designer release screenshot as the detail-page
  showcase and card image.
- Verify the website with the production Docusaurus build.

## Page and guide structure

The detail page will introduce Prisma Designer as a no-install WYSIWYG editor
for PrismaUI F4. Its primary external CTA will point to the Prisma Designer
GitHub repository, and its secondary CTA will point to the internal guide. The
page will explain the design-to-export workflow, the self-contained HTML output,
and the optional PrismaDesignerBridge. It will explicitly state that the live
preview validates rendering and layout, while real C++ listener wiring must be
verified through an exported view loaded by the user's plugin.

The guide will be an independent Docusaurus docs plugin with its own sidebar
and route base. It will be organized into these pages:

1. Getting Started
2. Canvas and View Types
3. Widgets, Templates, Icons, and Themes
4. Properties, Events, and Data Binding
5. Visual Scripting
6. Exporting HTML and C++ Integration
7. Live Game Preview with PrismaDesignerBridge
8. Validation, Project Files, and Troubleshooting

The guide content will be adapted from the current Prisma Designer README and
`docs/designer-guide.md`, preserving the actual filenames, export paths, API
names, limitations, and launcher instructions.

## Navigation and assets

The microsite entry will use `/tools/prismadesigner` as its path prefix and will
provide Guide and All Tools internal links plus GitHub and Releases external
links. The Tools card will use distinct badges such as `Browser Tool` and
`PrismaUI F4`, making it visually separate from Behavior Graph Studio.

The showcase asset will be copied from the existing Prisma Designer release
screenshots into `website/static/img/`. No new artwork or generated assets are
required.

## Verification

Run the website's dependency install/build workflow from `website/`. The build
must complete without broken-link errors. Confirm that the generated routes
include the Tools index, Prisma Designer detail page, and every guide page, and
that the new microsite navigation is selected for all Prisma Designer routes.

## Out of scope

- Changes to the Prisma Designer application itself.
- Changes to the PrismaDesignerBridge.
- Publishing or modifying the Prisma Designer repository.
- New screenshots, logos, or visual design systems.
