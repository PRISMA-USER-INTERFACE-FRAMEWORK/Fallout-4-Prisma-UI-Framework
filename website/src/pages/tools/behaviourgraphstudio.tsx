import React from 'react';
import Link from '@docusaurus/Link';
import useBaseUrl from '@docusaurus/useBaseUrl';
import Layout from '@theme/Layout';
import styles from './styles.module.css';

const GUIDE_PAGES = [
  {
    title: 'Getting Started',
    href: '/tools/behaviourgraphstudio/guide/getting-started',
    body: 'Install the tool, build a safe workspace, open files from disk or BA2 archives, and make your first verified edit.',
  },
  {
    title: 'Tree & Graph View',
    href: '/tools/behaviourgraphstudio/guide/tree-and-graph-view',
    body: 'Find objects quickly, frame selections, filter large files, and trace one state or generator through the graph.',
  },
  {
    title: 'Editing Nodes',
    href: '/tools/behaviourgraphstudio/guide/editing-nodes',
    body: 'Edit fields, add and reconnect graph objects, use Undo and Redo, copy subtrees, and build reusable templates.',
  },
  {
    title: 'Symbols & Variables',
    href: '/tools/behaviourgraphstudio/guide/symbols-and-variables',
    body: 'Manage variables, events, bounds, bindings, symbol usage, and the surrounding project chain.',
  },
  {
    title: 'Animation & Playback',
    href: '/tools/behaviourgraphstudio/guide/animation-and-playback',
    body: 'Inspect frames, filter bones, preview skeletons and supported meshes, check root motion, and diagnose timing problems.',
  },
  {
    title: 'Compare & Simulation',
    href: '/tools/behaviourgraphstudio/guide/compare-and-simulation',
    body: 'Compare edited files against originals and simulate events, variables, transitions, timing, and state changes.',
  },
  {
    title: 'Saving & Validating',
    href: '/tools/behaviourgraphstudio/guide/saving-and-validating',
    body: 'Run graph and project checks, understand save refusals, keep automatic backups, and prepare edits for in-game testing.',
  },
  {
    title: 'Troubleshooting & Tips',
    href: '/tools/behaviourgraphstudio/guide/troubleshooting-and-tips',
    body: 'Work through read-only files, missing meshes, unresolved animations, broken transitions, save refusals, and crash debugging.',
  },
];

export default function BehaviorGraphStudio(): JSX.Element {
  const screenshotUrl = useBaseUrl('/img/behaviourgraphstudio.webp');

  return (
    <Layout
      title="Behavior Graph Studio"
      description="A visual editor for Fallout 4's Havok behaviour graphs. Inspect, edit, validate, compare, preview, and save Fallout 4 HKX files."
    >
      <header className={styles.detailHero}>
        <div className={styles.detailHeroInner}>
          <div className={styles.badgeRow}>
            <span className={styles.badge}>.NET / Avalonia</span>
            <span className={styles.badge}>Beta</span>
          </div>
          <h1 className={styles.detailTitle}>Behavior Graph Studio</h1>
          <p className={styles.detailSub}>
            A standalone editor for Fallout 4&apos;s Havok behaviour graphs. Inspect the real object
            graph, edit fields and graph structure, manage symbols, preview animations, compare
            files, simulate state-machine logic, validate your project, and save supported changes
            straight back to <code>.hkx</code>.
          </p>
          <div className={styles.heroCta}>
            <a
              className={styles.ctaPrimary}
              href="https://www.nexusmods.com/fallout4/mods/107691"
              target="_blank"
              rel="noopener noreferrer"
            >
              Download on Nexus
            </a>
            <Link className={styles.ctaSecondary} to="/tools/behaviourgraphstudio/guide/getting-started">
              Read the guide
            </Link>
          </div>
        </div>
      </header>

      <main>
        {/* what it is */}
        <section className={styles.section}>
          <div className={styles.container}>
            <h2 className={styles.sectionTitle}>What it is</h2>
            <p className={styles.bodyText}>
              Fallout 4 keeps its animation logic in behaviour files: which clip plays, when it
              plays, how it blends, what events fire, and how states connect. Behavior Graph Studio
              reads the Fallout 4 Havok format directly and presents it as a tree, graph, symbol
              table, project chain, animation inspector, playback view, and comparison workspace.
              Supported edits use the built-in native save pipeline, with no Havok Content Tools,
              Java runtime, or game SDK required.
            </p>
            <div className={styles.showcaseImgWrap}>
              <img
                src={screenshotUrl}
                alt="Behavior Graph Studio's playback view rendering Dogmeat's wireframe skeleton"
                className={styles.showcaseImg}
              />
            </div>
          </div>
        </section>

        {/* guide */}
        <section className={styles.section}>
          <div className={styles.container}>
            <h2 className={styles.sectionTitle}>Guide</h2>
            <p className={styles.bodyText}>
              A full workflow from opening your first file to tracing graph logic, previewing
              animations, validating changes, comparing the final result, and testing safely in
              Fallout 4.
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
            <div className={styles.callout}>
              <strong>Safe editing:</strong> supported field and structural edits use the native
              save pipeline, and saving keeps the previous file as a <code>.bak</code>. If an edit
              cannot be represented safely, the application refuses the save instead of replacing
              the source file. Keep your backup until the edited behaviour has been tested in game.
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
