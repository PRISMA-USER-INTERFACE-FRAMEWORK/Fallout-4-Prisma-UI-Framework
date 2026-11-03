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
    label: 'Make a UI',
    title: 'Real HTML, inside Fallout',
    body: 'Buttons, sliders, tabs, HUD overlays: the web tools you already know, rendered in the game by an in-process Ultralight view.',
  },
  {
    label: 'Controller players',
    title: 'Controller support that fits',
    body: 'Map buttons to your own actions, draw matching prompts, and keep D-pad, A and B navigation working without extra code.',
  },
  {
    label: 'Talk to your plugin',
    title: 'Two-way bridge',
    body: 'Send JSON to the page, receive clicks and commands back on the game thread, and change game state safely from a button.',
  },
  {
    label: 'Many mods, one screen',
    title: 'Plays well with other mods',
    body: 'Declare your panel, check whether another Prisma mod is already open, and never fight another graphics or UI injector.',
  },
];

export default function Home(): JSX.Element {
  const gettingStartedUrl = useBaseUrl('/docs/getting-started');
  const quickStartUrl = useBaseUrl('/docs/quick-start');
  const cookbookUrl = useBaseUrl('/docs/cookbook');
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
          <p className={styles.heroTagline}>Build Fallout 4 menus and HUDs with HTML, CSS and JavaScript</p>
          <p className={styles.heroSub}>Write a normal web page, show it from your plugin, and keep the game running underneath. This site walks you from install to a working panel, with complete examples for every feature.</p>
          <div className={styles.heroCta}>
            <Link className={styles.ctaPrimary} to={quickStartUrl}>Your first panel</Link>
            <Link className={styles.ctaSecondary} to={gettingStartedUrl}>Install PrismaUI</Link>
            <Link className={styles.ctaSecondary} to={cookbookUrl}>Cookbook</Link>
            <Link className={styles.ctaSecondary} to={apiReferenceUrl}>API reference</Link>
            <a className={styles.ctaNexus} href="https://www.nexusmods.com/fallout4/mods/105454" target="_blank" rel="noopener noreferrer">Download on Nexus</a>
          </div>
        </div>
      </header>

      <main>
        <section className={styles.howItWorks}>
          <div className={styles.container}>
            <h2 className={styles.sectionTitle}>How it works</h2>
            <p className={styles.sectionSub}>You write a normal web page. Your plugin tells PrismaUI when to show it and what to put in it. PrismaUI draws it in Fallout's own frame and routes the mouse, keyboard and controller input back to the page.</p>
            <div className={styles.arch}>
              <div className={styles.archBox}><span className={styles.archLabel}>You write</span><strong>HTML / CSS / JS</strong><span className={styles.archNote}>Plain web files, shipped with your mod.</span></div>
              <div className={styles.archArrow}>→</div>
              <div className={`${styles.archBox} ${styles.archBoxCore}`}><span className={styles.archLabel}>Runs inside</span><strong>PrismaUI F4</strong><span className={styles.archNote}>Ultralight 1.4.0 in the game process. No browser to launch.</span></div>
              <div className={styles.archArrow}>→</div>
              <div className={styles.archBox}><span className={styles.archLabel}>Talks to</span><strong>Your plugin</strong><span className={styles.archNote}>JSON, events, controller actions and game-thread work.</span></div>
            </div>
          </div>
        </section>

        <section className={styles.features}><div className={styles.container}><div className={styles.featureGrid}>{FEATURES.map((f) => (<div key={f.label} className={styles.featureCard}><span className={styles.featureLabel}>{f.label}</span><h3 className={styles.featureTitle}>{f.title}</h3><p className={styles.featureDesc}>{f.body}</p></div>))}</div></div></section>

        <section className={styles.showcase}><div className={styles.container}><h2 className={styles.sectionTitle}>Made for in-game screens</h2><p className={styles.sectionSub}>Menus, HUDs, terminals and world-space screens — without touching a Scaleform SWF.</p><div className={styles.showcaseImgWrap}><img src={useBaseUrl('/img/showcase-hud.webp')} alt="PrismaUI rendering an HTML interface in Fallout 4" className={styles.showcaseImg} /></div></div></section>

        <section className={styles.quickStart}><div className={styles.container}><h2 className={styles.sectionTitle}>Ten minutes to a working panel</h2><p className={styles.sectionSub}>Install the framework, copy one header, then follow the first-panel walkthrough. When it works, copy a recipe from the cookbook for the thing you actually want to build.</p><CodeBlock language="cpp">{QUICK_START}</CodeBlock><Link className={styles.sectionLink} to={quickStartUrl}>Build your first panel →</Link>
          <Link className={styles.sectionLink} to={cookbookUrl}>Browse the cookbook →</Link></div></section>

        <section className={styles.mcpSection}><div className={styles.container}><div className={styles.mcpCard}><div className={styles.mcpText}><h2 className={styles.mcpTitle}>Look things up when you need them</h2><p className={styles.mcpDesc}>Every call has its own page with the signature, parameters and an example. The guides cover controller, localization and game-thread work, and the repository also ships the headers and a small MCP server if you want your editor to query the API.</p><div className={styles.mcpActions}><Link className={styles.ctaPrimary} to={apiReferenceUrl}>Browse V1-V12</Link><Link className={styles.ctaSecondary} to={controllerUrl}>Controller actions</Link><a className={styles.ctaSecondary} href="https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Fallout-4-Prisma-UI-Framework/tree/main/mcp-server" target="_blank" rel="noopener noreferrer">Prisma MCP</a></div></div></div></div></section>
      </main>
    </Layout>
  );
}
