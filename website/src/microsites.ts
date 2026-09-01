export type MicrositeLink = {
  label: string;
  to?: string;
  href?: string;
};

export type Microsite = {
  /** Path, relative to baseUrl, that this microsite's pages live under. */
  pathPrefix: string;
  name: string;
  homeTo: string;
  /** Internal links shown in the navbar, next to the brand. */
  navItems: MicrositeLink[];
  /** External links (Nexus, Discord, GitHub, ...) shown on the right of the navbar. */
  externalNavItems: MicrositeLink[];
  footerLinks: MicrositeLink[];
  copyrightName: string;
};

/**
 * Tools that get their own navbar/footer instead of the main PrismaUI F4 one.
 * Add another entry here to give a future tool the same "separate site" treatment.
 */
export const MICROSITES: Microsite[] = [
  {
    pathPrefix: '/tools/behaviourgraphstudio',
    name: 'Behavior Graph Studio',
    homeTo: '/tools/behaviourgraphstudio',
    navItems: [
      {label: 'Guide', to: '/tools/behaviourgraphstudio/guide/getting-started'},
      {label: 'All Tools', to: '/tools'},
    ],
    externalNavItems: [
      {label: 'Nexus', href: 'https://www.nexusmods.com/fallout4/mods/107691'},
      {label: 'Discord', href: 'https://discord.gg/cPmT8SmW4D'},
      {label: 'GitHub', href: 'https://github.com/NomadsReach/BehaviorGraphStudio'},
    ],
    footerLinks: [
      {label: 'Guide', to: '/tools/behaviourgraphstudio/guide/getting-started'},
      {label: 'Nexus Mods', href: 'https://www.nexusmods.com/fallout4/mods/107691'},
      {label: 'Discord', href: 'https://discord.gg/cPmT8SmW4D'},
      {label: 'GitHub', href: 'https://github.com/NomadsReach/BehaviorGraphStudio'},
    ],
    copyrightName: 'Behavior Graph Studio',
  },
  {
    pathPrefix: '/tools/prismadesigner',
    name: 'Prisma Designer',
    homeTo: '/tools/prismadesigner',
    navItems: [
      {label: 'Guide', to: '/tools/prismadesigner/guide/getting-started'},
      {label: 'All Tools', to: '/tools'},
    ],
    externalNavItems: [
      {
        label: 'GitHub',
        href: 'https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Prisma-Designer',
      },
      {
        label: 'Releases',
        href: 'https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Prisma-Designer/releases',
      },
    ],
    footerLinks: [
      {label: 'Guide', to: '/tools/prismadesigner/guide/getting-started'},
      {
        label: 'GitHub',
        href: 'https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Prisma-Designer',
      },
      {
        label: 'Releases',
        href: 'https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Prisma-Designer/releases',
      },
    ],
    copyrightName: 'Prisma Designer',
  },
];

function joinPath(baseUrl: string, path: string): string {
  return baseUrl.replace(/\/$/, '') + path;
}

export function findMicrosite(pathname: string, baseUrl: string): Microsite | undefined {
  return MICROSITES.find((m) => pathname.startsWith(joinPath(baseUrl, m.pathPrefix)));
}
