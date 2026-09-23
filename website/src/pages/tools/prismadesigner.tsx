import React from 'react';
import Link from '@docusaurus/Link';
import useBaseUrl from '@docusaurus/useBaseUrl';
import Layout from '@theme/Layout';
import styles from './styles.module.css';

const GUIDE_PAGES = [
  {
    title: 'Getting Started',
    href: '/tools/prismadesigner/guide/getting-started',
    body: 'Open the static editor, create your first view, and export it into a PrismaUI F4 plugin.',
  },
  {
    title: 'Canvas and View Types',
    href: '/tools/prismadesigner/guide/canvas-and-view-types',
    body: 'Set the target resolution, navigate the canvas, and arrange elements with pixel precision.',
  },
  {
    title: 'Widgets, Templates, Icons, and Themes',
    href: '/tools/prismadesigner/guide/widgets-templates-icons-themes',
    body: 'Choose from game-focused widgets, reusable presets, searchable icons, and Fallout-themed palettes.',
  },
  {
    title: 'Properties, Events, and Data Binding',
    href: '/tools/prismadesigner/guide/properties-events-and-binding',
    body: 'Style elements, wire buttons to C++ listeners, and bind views to live game values.',
  },
  {
    title: 'Visual Scripting',
    href: '/tools/prismadesigner/guide/visual-scripting',
    body: 'Build event-driven behavior that compiles to plain JavaScript inside the exported HTML.',
  },
  {
    title: 'Exporting HTML and C++ Integration',
    href: '/tools/prismadesigner/guide/exporting-and-cpp-integration',
    body: 'Export a self-contained view and load it from an F4SE plugin with the PrismaUI API.',
  },
  {
    title: 'Live Game Preview',
    href: '/tools/prismadesigner/guide/live-game-preview',
    body: 'Connect the optional Bridge to preview layout and live values in a running Fallout 4 game.',
  },
  {
    title: 'Validation and Troubleshooting',
    href: '/tools/prismadesigner/guide/validation-project-files-and-troubleshooting',
    body: 'Keep projects editable, understand validation findings, and diagnose common integration problems.',
  },
];

export default function PrismaDesigner(): JSX.Element {
  const screenshotUrl = useBaseUrl('/img/prisma-designer-main-menu.png');

  return (
    <Layout
      title="Prisma Designer"
      description="A no-install visual editor for building HTML, CSS, and JavaScript views for PrismaUI F4."
    >
      <header className={styles.detailHero}>
        <div className={styles.detailHeroInner}>
          <div className={styles.badgeRow}>
            <span className={styles.badge}>Browser Tool</span>
            <span className={styles.badge}>PrismaUI F4</span>
            <span className={styles.badge}>No install</span>
          </div>
          <h1 className={styles.detailTitle}>Prisma Designer</h1>
          <p className={styles.detailSub}>
            A standalone WYSIWYG editor for building Fallout 4 menus, HUDs,
            Pip-Boy screens, and cursor views for PrismaUI F4. Design visually,
            export a self-contained HTML file, and load it in your plugin.
          </p>
          <div className={styles.heroCta}>
            <a
              className={styles.ctaPrimary}
              href="https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Prisma-Designer/releases"
              target="_blank"
              rel="noopener noreferrer"
            >
              Download from GitHub
            </a>
            <Link className={styles.ctaSecondary} to="/tools/prismadesigner/guide/getting-started">
              Read the guide
            </Link>
          </div>
        </div>
      </header>

      <main>
        <section className={styles.section}>
          <div className={styles.container}>
            <h2 className={styles.sectionTitle}>Design the view, not the boilerplate</h2>
            <p className={styles.bodyText}>
              Prisma Designer runs directly from <code>index.html</code> in Chrome or Edge.
              Add components to a target-resolution canvas, edit their properties in the
              inspector, and save projects as reusable <code>.prisma</code> files. The editor
              includes 24 element types, built-in Fallout-themed presets, 4,180 searchable SVG
              icons, and a scene outliner for larger layouts.
            </p>
            <div className={styles.showcaseImgWrap}>
              <img
                src={screenshotUrl}
                alt="Prisma Designer showing a Fallout 4 main menu layout on the canvas"
                className={styles.showcaseImg}
              />
            </div>
          </div>
        </section>

        <section className={styles.section}>
          <div className={styles.container}>
            <h2 className={styles.sectionTitle}>From canvas to game</h2>
            <p className={styles.bodyText}>
              The exported view is self-contained. Coordinates, styling, images, canvas draw
              functions, button stubs, and visual scripting are all written into one HTML file.
              Put it under <code>Data/PrismaUI_F4/views/&lt;YourPlugin&gt;/</code> and load it with
              <code> CreateView</code> using a path relative to the Prisma views root. PrismaUI_F4
              2.1.0 does not require an extra <code>Interface</code> subdirectory. The export dialog
              also prints the C++ listener code for buttons configured in the designer.
            </p>
            <div className={styles.callout}>
              <strong>Bridge preview scope:</strong> PrismaDesignerBridge previews rendering,
              layout, and supported live values inside a running game. It does not register your
              plugin&apos;s C++ listeners. Verify real button behavior by exporting the view and
              loading it from your own plugin.
            </div>
          </div>
        </section>

        <section className={styles.section}>
          <div className={styles.container}>
            <h2 className={styles.sectionTitle}>Full guide</h2>
            <p className={styles.bodyText}>
              Follow the complete workflow, from opening the editor to validating an exported
              view in Fallout 4.
            </p>
            <div className={styles.cardGrid} style={{marginTop: 24}}>
              {GUIDE_PAGES.map((page) => (
                <Link key={page.href} to={page.href} className={styles.card}>
                  <div className={styles.cardBody}>
                    <h3 className={styles.cardTitle} style={{fontSize: '1.1rem'}}>
                      {page.title}
                    </h3>
                    <p className={styles.cardDesc}>{page.body}</p>
                    <span className={styles.cardLink}>Read →</span>
                  </div>
                </Link>
              ))}
            </div>
          </div>
        </section>

        <div className={styles.container}>
          <Link className={styles.backLink} to="/tools">
            ← Back to Tools
          </Link>
        </div>
      </main>
    </Layout>
  );
}
