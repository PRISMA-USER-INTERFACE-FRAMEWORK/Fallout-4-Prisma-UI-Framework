import React from 'react';
import {useLocation} from '@docusaurus/router';
import useDocusaurusContext from '@docusaurus/useDocusaurusContext';
import FaqWidget from '@site/src/components/FaqWidget';
import {findMicrosite} from '@site/src/microsites';

// Docusaurus renders this around the whole app, so the FAQ launcher shows on every page
// except a microsite's own pages, since it only knows about PrismaUI F4.
export default function Root({ children }: { children: React.ReactNode }): JSX.Element {
  const {pathname} = useLocation();
  const {siteConfig} = useDocusaurusContext();
  const isMicrosite = Boolean(findMicrosite(pathname, siteConfig.baseUrl));

  return (
    <>
      {children}
      {!isMicrosite && <FaqWidget />}
    </>
  );
}
