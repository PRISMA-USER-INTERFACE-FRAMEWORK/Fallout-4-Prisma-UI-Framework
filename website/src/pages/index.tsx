import React from 'react';
import Link from '@docusaurus/Link';
import useBaseUrl from '@docusaurus/useBaseUrl';
import Layout from '@theme/Layout';
import CodeBlock from '@theme/CodeBlock';
import styles from './index.module.css';

const QUICK_START = `#include "PrismaUI_F4_Modern_API.h"

using namespace PRISMA_UI_FLAT_API;

static ControllerAPI g_controller{};

bool LoadPrismaController()
{
    return Discover<ApiFeature::Controller>(ControllerApiVersion, g_controller);
}`;

const FEATURES = [
  {
    label: 'Current desktop API',
    title: 'Feature tables + V1-V12',
    body: 'New integrations can discover Core, Controller, GameThread, Meta, View, Interop, Localization, Render, Input, and Menu tables while the V1-V12 ABI remains stable.',
  },
  {
    label: 'Controller support',
    title: 'Framework-owned actions',
    body: 'Bind semantic controller actions, use native Gamepad state when needed, and let PrismaUI own routing, repeat state, bridge installation, and bounded recovery.',
  },
  {
    label: 'In-process runtime',
    title: 'Ultralight 1.4.0',
    body: 'PrismaUI runs Ultralight inside Fallout 4 with the rewritten D3D11 GPU-accelerated presentation path and a controlled CPU BitmapSurface fallback.',
  },
  {
    label: 'Web authoring',
    title: 'HTML / CSS / JavaScript',
    body: 'Use vanilla web assets or a compiled React, Vue, or Svelte build. Bundle required dependencies locally with your mod.',
  },
];

export default function Home(): JSX.Element {
  const gettingStartedUrl = useBaseUrl('/docs/getting-started');
  const apiReferenceUrl = useBaseUrl('/docs/api-reference');
  const controllerUrl = useBaseUrl('/docs/controller-actions');
  const modernApiUrl = useBaseUrl('/docs/modern-api');
  const logoUrl = useBaseUrl('/img/prisma-logo.png');

  return (
    <Layout
      title="PrismaUI F4 HTML/JS UI framework for Fallout 4"
      description="Build Fallout 4 interfaces with HTML, CSS and JavaScript using PrismaUI, Ultralight and the current V1-V12 native API."
    >
      <header className={styles.hero}>
        <div className={styles.heroInner}>
          <img src={logoUrl} alt="PrismaUI F4" className={styles.heroLogo} />
          <span className={styles.versionBadge}>PrismaUI 2.2.0</span>
          <h1 className={styles.heroTitle}>PrismaUI F4</h1>
          <p className={styles.heroTagline}>HTML, CSS, and JavaScript UI framework for Fallout 4</p>
          <p className={styles.heroSub}>Ultralight 1.4.0 in-process. Modern feature discovery. V4 localization. Native controller routing. In-game DevTools.</p>
          <div className={styles.heroCta}>
            <Link className={styles.ctaPrimary} to={gettingStartedUrl}>Get started</Link>
            <Link className={styles.ctaSecondary} to={modernApiUrl}>Modern API</Link>
            <Link className={styles.ctaSecondary} to={controllerUrl}>Controller guide</Link>
            <Link className={styles.ctaSecondary} to={apiReferenceUrl}>Legacy V1-V12</Link>
            <a className={styles.ctaNexus} href="https://www.nexusmods.com/fallout4/mods/105454" target="_blank" rel="noopener noreferrer">Download on Nexus</a>
          </div>
        </div>
      </header>

      <main>
        <section className={styles.howItWorks}>
          <div className={styles.container}>
            <h2 className={styles.sectionTitle}>Current architecture</h2>
            <p className={styles.sectionSub}>Build local web assets. PrismaUI renders them in Fallout 4 and exposes a versioned native bridge for lifecycle, focus, engine-thread work, input, controller actions, prompts, translations and game UI integration.</p>
            <div className={styles.arch}>
              <div className={styles.archBox}><span className={styles.archLabel}>You write</span><strong>HTML / CSS / JS</strong><span className={styles.archNote}>Local static assets owned by your mod.</span></div>
              <div className={styles.archArrow}>→</div>
              <div className={`${styles.archBox} ${styles.archBoxCore}`}><span className={styles.archLabel}>Renders via</span><strong>PrismaUI F4</strong><span className={styles.archNote}>Ultralight 1.4.0 in-process with D3D11 GPU presentation and controlled CPU fallback.</span></div>
              <div className={styles.archArrow}>→</div>
              <div className={styles.archBox}><span className={styles.archLabel}>Integrates with</span><strong>Fallout 4 / F4SE</strong><span className={styles.archNote}>Focus, input, controller routing and native engine handoff.</span></div>
            </div>
          </div>
        </section>

        <section className={styles.features}><div className={styles.container}><div className={styles.featureGrid}>{FEATURES.map((f) => (<div key={f.label} className={styles.featureCard}><span className={styles.featureLabel}>{f.label}</span><h3 className={styles.featureTitle}>{f.title}</h3><p className={styles.featureDesc}>{f.body}</p></div>))}</div></div></section>

        <section className={styles.showcase}><div className={styles.container}><h2 className={styles.sectionTitle}>Runs in the game world</h2><p className={styles.sectionSub}>Build overlays and panels without editing Scaleform SWFs.</p><div className={styles.showcaseImgWrap}><img src={useBaseUrl('/img/showcase-hud.webp')} alt="PrismaUI rendering an HTML interface in Fallout 4" className={styles.showcaseImg} /></div></div></section>

        <section className={styles.quickStart}><div className={styles.container}><h2 className={styles.sectionTitle}>Start with feature discovery</h2><p className={styles.sectionSub}>Include PrismaUI_F4_Modern_API.h, discover only the feature table you need, and let runtime capability checks decide what is available. Existing V1-V12 consumers remain supported.</p><CodeBlock language="cpp">{QUICK_START}</CodeBlock><Link className={styles.sectionLink} to={modernApiUrl}>Read the modern API guide →</Link></div></section>

        <section className={styles.mcpSection}><div className={styles.container}><div className={styles.mcpCard}><div className={styles.mcpText}><h2 className={styles.mcpTitle}>Developer reference and Prisma MCP</h2><p className={styles.mcpDesc}>The public repository carries the canonical legacy and modern SDK headers, per-method reference pages, current controller, localization and threading guides, and prisma-mcp so tooling can query the same verified API contract instead of guessing from older PrismaUI material.</p><div className={styles.mcpActions}><Link className={styles.ctaPrimary} to={apiReferenceUrl}>Browse V1-V12</Link><Link className={styles.ctaSecondary} to={controllerUrl}>Controller actions</Link><a className={styles.ctaSecondary} href="https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Fallout-4-Prisma-UI-Framework/tree/main/mcp-server" target="_blank" rel="noopener noreferrer">Prisma MCP</a></div></div></div></div></section>
      </main>
    </Layout>
  );
}
