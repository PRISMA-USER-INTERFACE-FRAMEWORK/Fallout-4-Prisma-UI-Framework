import { themes as prismThemes } from 'prism-react-renderer';
import type { Config } from '@docusaurus/types';
import type * as Preset from '@docusaurus/preset-classic';

const config: Config = {
  title: 'PrismaUI F4',
  tagline: 'HTML/CSS/JS UI framework for Fallout 4',
  url: 'https://prisma-user-interface-framework.github.io',
  baseUrl: '/Fallout-4-Prisma-UI-Framework/',
  organizationName: 'PRISMA-USER-INTERFACE-FRAMEWORK',
  projectName: 'Fallout-4-Prisma-UI-Framework',
  onBrokenLinks: 'throw',
  markdown: {
    hooks: {
      onBrokenMarkdownLinks: 'warn',
    },
  },
  i18n: {
    defaultLocale: 'en',
    locales: ['en'],
  },

  presets: [
    [
      'classic',
      {
        docs: {
          sidebarPath: './sidebars.ts',
        },
        blog: false,
        theme: {
          customCss: './src/css/custom.css',
        },
      } satisfies Preset.Options,
    ],
  ],

  plugins: [
    [
      '@easyops-cn/docusaurus-search-local',
      {
        hashed: true,
        docsRouteBasePath: '/docs',
      },
    ],
    './plugins/source-docs',
    './plugins/llms-txt',
    [
      '@docusaurus/plugin-content-docs',
      {
        id: 'bgs',
        path: 'bgs-docs',
        routeBasePath: 'tools/behaviourgraphstudio/guide',
        sidebarPath: './bgsSidebars.ts',
      },
    ],
    [
      '@docusaurus/plugin-content-docs',
      {
        id: 'designer',
        path: 'designer-docs',
        routeBasePath: 'tools/prismadesigner/guide',
        sidebarPath: './designerSidebars.ts',
      },
    ],
  ],

  themeConfig: {
    colorMode: {
      defaultMode: 'dark',
      disableSwitch: false,
    },
    navbar: {
      title: 'PrismaUI F4',
      items: [
        {
          type: 'docSidebar',
          sidebarId: 'guideSidebar',
          position: 'left',
          label: 'Guides',
        },
        {
          type: 'docSidebar',
          sidebarId: 'apiSidebar',
          position: 'left',
          label: 'API Reference',
        },
        {
          to: '/tools',
          position: 'left',
          label: 'Tools',
        },
        {
          href: 'https://www.nexusmods.com/fallout4/mods/105454',
          label: 'Nexus',
          position: 'right',
        },
        {
          href: 'https://discord.com/invite/bawdketrFX',
          label: 'Discord',
          position: 'right',
        },
        {
          href: 'https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Fallout-4-Prisma-UI-Framework',
          label: 'GitHub',
          position: 'right',
        },
      ],
    },
    footer: {
      style: 'dark',
      links: [
        {
          title: 'Docs',
          items: [
            { label: 'Getting Started', to: '/docs/getting-started' },
            { label: 'API Reference', to: '/docs/api-reference' },
            { label: 'Changelog', to: '/docs/changelog' },
          ],
        },
        {
          title: 'Links',
          items: [
            {
              label: 'GitHub',
              href: 'https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Fallout-4-Prisma-UI-Framework',
            },
            {
              label: 'Nexus Mods',
              href: 'https://www.nexusmods.com/fallout4/mods/105454',
            },
            {
              label: 'Discord',
              href: 'https://discord.com/invite/bawdketrFX',
            },
          ],
        },
      ],
      copyright: `Copyright © ${new Date().getFullYear()} PRISMA UI. Built with Docusaurus.`,
    },
    prism: {
      theme: prismThemes.github,
      darkTheme: prismThemes.dracula,
      additionalLanguages: ['powershell', 'cpp', 'lua', 'json'],
    },
  } satisfies Preset.ThemeConfig,
};

export default config;
