import React from 'react';
import Link from '@docusaurus/Link';
import useBaseUrl from '@docusaurus/useBaseUrl';
import Layout from '@theme/Layout';
import styles from './styles.module.css';

export default function ToolsIndex(): JSX.Element {
  const prismaLogo = useBaseUrl('/img/prisma-logo.png');
  const behaviorGraphStudioImage = useBaseUrl('/img/behaviourgraphstudio.webp');
  const prismaDesignerImage = useBaseUrl('/img/prisma-designer-main-menu.png');

  const TOOLS = [
    {
      id: 'prisma',
      name: 'PrismaUI F4',
      tags: ['F4SE Framework', 'v2.1.0'],
      pitch: 'HTML, CSS, and JavaScript UI framework for Fallout 4, powered by in-process Ultralight 1.4.0.',
      href: '/',
      image: prismaLogo,
      imageFit: 'contain' as const,
    },
    {
      id: 'behaviorgraphstudio',
      name: 'Behavior Graph Studio',
      tags: ['.NET / Avalonia', 'Modding Tool'],
      pitch: "A visual editor for Fallout 4's Havok behaviour graphs. Open, edit, and save animation state machines directly, no Havok Content Tools required.",
      href: '/tools/behaviourgraphstudio',
      image: behaviorGraphStudioImage,
      imageFit: 'cover' as const,
    },
    {
      id: 'prismadesigner',
      name: 'Prisma Designer',
      tags: ['Browser Tool', 'PrismaUI F4'],
      pitch: 'A no-install visual editor for building HTML/CSS/JavaScript views for PrismaUI F4, with export, binding, visual scripting, and optional live game preview.',
      href: '/tools/prismadesigner',
      image: prismaDesignerImage,
      imageFit: 'cover' as const,
    },
  ];

  return (
    <Layout
      title="Tools"
      description="Tools built by the PrismaUI F4 team for Fallout 4 modding."
    >
      <header className={styles.hero}>
        <div className={styles.heroInner}>
          <span className={styles.eyebrow}>For Modders</span>
          <h1 className={styles.heroTitle}>Tools</h1>
          <p className={styles.heroSub}>
            PrismaUI F4 and the other tools we build alongside it for Fallout 4 modding.
          </p>
        </div>
      </header>

      <main>
        <section className={styles.gridSection}>
          <div className={styles.container}>
            <div className={styles.cardGrid}>
              {TOOLS.map((tool) => (
                <Link key={tool.id} to={tool.href} className={styles.card}>
                  <div
                    className={styles.cardImageWrap}
                    style={tool.imageFit === 'contain' ? {background: '#000'} : undefined}
                  >
                    <img
                      src={tool.image}
                      alt={tool.name}
                      className={styles.cardImage}
                      style={{objectFit: tool.imageFit}}
                    />
                  </div>
                  <div className={styles.cardBody}>
                    <div className={styles.tagRow}>
                      {tool.tags.map((tag) => (
                        <span key={tag} className={styles.tag}>
                          {tag}
                        </span>
                      ))}
                    </div>
                    <h2 className={styles.cardTitle}>{tool.name}</h2>
                    <p className={styles.cardDesc}>{tool.pitch}</p>
                    <span className={styles.cardLink}>View tool →</span>
                  </div>
                </Link>
              ))}
            </div>
          </div>
        </section>
      </main>
    </Layout>
  );
}
