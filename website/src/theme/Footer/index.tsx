import React, {type ReactNode} from 'react';
import Link from '@docusaurus/Link';
import {useLocation} from '@docusaurus/router';
import useDocusaurusContext from '@docusaurus/useDocusaurusContext';

import {useThemeConfig} from '@docusaurus/theme-common';
import FooterLinks from '@theme/Footer/Links';
import FooterLogo from '@theme/Footer/Logo';
import FooterCopyright from '@theme/Footer/Copyright';
import FooterLayout from '@theme/Footer/Layout';
import {findMicrosite} from '@site/src/microsites';

function MicrositeFooter({
  microsite,
}: {
  microsite: NonNullable<ReturnType<typeof findMicrosite>>;
}): ReactNode {
  return (
    <FooterLayout
      style="dark"
      links={
        <div className="row footer__links">
          <div className="col footer__col">
            <div className="footer__title">{microsite.name}</div>
            <ul className="footer__items clean-list">
              {microsite.footerLinks.map((item) => (
                <li className="footer__item" key={item.label}>
                  {item.to ? (
                    <Link to={item.to} className="footer__link-item">
                      {item.label}
                    </Link>
                  ) : (
                    <a
                      href={item.href}
                      target="_blank"
                      rel="noopener noreferrer"
                      className="footer__link-item">
                      {item.label}
                    </a>
                  )}
                </li>
              ))}
            </ul>
          </div>
        </div>
      }
      copyright={
        <div className="footer__copyright">
          Copyright © {new Date().getFullYear()} {microsite.copyrightName}. Built with
          Docusaurus.
        </div>
      }
    />
  );
}

function Footer(): ReactNode {
  const {footer} = useThemeConfig();
  const {pathname} = useLocation();
  const {siteConfig} = useDocusaurusContext();
  const microsite = findMicrosite(pathname, siteConfig.baseUrl);

  if (microsite) {
    return <MicrositeFooter microsite={microsite} />;
  }

  if (!footer) {
    return null;
  }
  const {copyright, links, logo, style} = footer;

  return (
    <FooterLayout
      style={style}
      links={links && links.length > 0 && <FooterLinks links={links} />}
      logo={logo && <FooterLogo logo={logo} />}
      copyright={copyright && <FooterCopyright copyright={copyright} />}
    />
  );
}

export default React.memo(Footer);
