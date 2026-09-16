import React, {type ReactNode} from 'react';
import Link from '@docusaurus/Link';
import {useLocation} from '@docusaurus/router';
import useDocusaurusContext from '@docusaurus/useDocusaurusContext';
import {useNavbarMobileSidebar} from '@docusaurus/theme-common/internal';
import {translate} from '@docusaurus/Translate';
import NavbarColorModeToggle from '@theme/Navbar/ColorModeToggle';
import IconClose from '@theme/Icon/Close';
import NavbarLogo from '@theme/Navbar/Logo';
import {findMicrosite} from '@site/src/microsites';

function CloseButton() {
  const mobileSidebar = useNavbarMobileSidebar();
  return (
    <button
      type="button"
      aria-label={translate({
        id: 'theme.docs.sidebar.closeSidebarButtonAriaLabel',
        message: 'Close navigation bar',
        description: 'The ARIA label for close button of mobile sidebar',
      })}
      className="clean-btn navbar-sidebar__close"
      onClick={() => mobileSidebar.toggle()}>
      <IconClose color="var(--ifm-color-emphasis-600)" />
    </button>
  );
}

export default function NavbarMobileSidebarHeader(): ReactNode {
  const {pathname} = useLocation();
  const {siteConfig} = useDocusaurusContext();
  const microsite = findMicrosite(pathname, siteConfig.baseUrl);

  return (
    <div className="navbar-sidebar__brand">
      {microsite ? (
        <Link to={microsite.homeTo} className="navbar__brand">
          <span className="navbar__title">{microsite.name}</span>
        </Link>
      ) : (
        <NavbarLogo />
      )}
      <NavbarColorModeToggle className="margin-right--md" />
      <CloseButton />
    </div>
  );
}
