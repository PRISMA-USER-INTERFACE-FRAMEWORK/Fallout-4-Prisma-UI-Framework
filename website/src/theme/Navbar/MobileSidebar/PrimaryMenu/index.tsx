import React, {type ReactNode} from 'react';
import Link from '@docusaurus/Link';
import {useLocation} from '@docusaurus/router';
import useDocusaurusContext from '@docusaurus/useDocusaurusContext';
import {useThemeConfig} from '@docusaurus/theme-common';
import {useNavbarMobileSidebar} from '@docusaurus/theme-common/internal';
import NavbarItem, {type Props as NavbarItemConfig} from '@theme/NavbarItem';
import {findMicrosite, type MicrositeLink} from '@site/src/microsites';

function useNavbarItems() {
  // TODO temporary casting until ThemeConfig type is improved
  return useThemeConfig().navbar.items as NavbarItemConfig[];
}

function MicrositeMenuItem({
  item,
  onClick,
}: {
  item: MicrositeLink;
  onClick: () => void;
}) {
  return (
    <li className="menu__list-item">
      {item.to ? (
        <Link className="menu__link" to={item.to} onClick={onClick}>
          {item.label}
        </Link>
      ) : (
        <a
          className="menu__link"
          href={item.href}
          target="_blank"
          rel="noopener noreferrer"
          onClick={onClick}>
          {item.label}
        </a>
      )}
    </li>
  );
}

// The primary menu displays the navbar items
export default function NavbarMobilePrimaryMenu(): ReactNode {
  const mobileSidebar = useNavbarMobileSidebar();
  const {pathname} = useLocation();
  const {siteConfig} = useDocusaurusContext();
  const microsite = findMicrosite(pathname, siteConfig.baseUrl);

  // TODO how can the order be defined for mobile?
  // Should we allow providing a different list of items?
  const items = useNavbarItems();

  if (microsite) {
    const onClick = () => mobileSidebar.toggle();
    return (
      <ul className="menu__list">
        {[...microsite.navItems, ...microsite.externalNavItems].map((item) => (
          <MicrositeMenuItem key={item.label} item={item} onClick={onClick} />
        ))}
      </ul>
    );
  }

  return (
    <ul className="menu__list">
      {items.map((item, i) => (
        <NavbarItem
          mobile
          {...item}
          onClick={() => mobileSidebar.toggle()}
          key={i}
        />
      ))}
    </ul>
  );
}
