import React, { useState } from 'react';
import { useDoc } from '@docusaurus/plugin-content-docs/client';
import useBaseUrl from '@docusaurus/useBaseUrl';
import styles from './styles.module.css';

type CopyState = 'idle' | 'loading' | 'done' | 'error';

type Provider = {
  label: string;
  url: string;
};

const PROVIDERS: Provider[] = [
  { label: 'Open ChatGPT', url: 'https://chatgpt.com/' },
  { label: 'Open Claude', url: 'https://claude.ai/chat' },
  { label: 'Open Gemini', url: 'https://gemini.google.com/' },
];

const MCP_SETUP = `git clone https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Fallout-4-Prisma-UI-Framework.git
cd Fallout-4-Prisma-UI-Framework/mcp-server
npm ci
npm run build`;

const MCP_CONFIG = `{
  "mcpServers": {
    "prisma-mcp": {
      "command": "node",
      "args": [
        "/ABSOLUTE/PATH/Fallout-4-Prisma-UI-Framework/mcp-server/dist/index.js"
      ]
    }
  }
}`;

export default function AiToolbar(): JSX.Element {
  const { metadata } = useDoc();
  const sourceDocPath = useBaseUrl(`/docs/${metadata.id}.md`);
  const mcpGuidePath = useBaseUrl('/docs/ai-mcp');

  const [copyState, setCopyState] = useState<CopyState>('idle');
  const [modalOpen, setModalOpen] = useState(false);
  const [snippetCopied, setSnippetCopied] = useState<'setup' | 'config' | null>(null);
  const [askStatus, setAskStatus] = useState('');

  async function fetchMarkdown(): Promise<string> {
    const response = await fetch(sourceDocPath);
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    return response.text();
  }

  async function copyCurrentPage(): Promise<void> {
    const markdown = await fetchMarkdown();
    await navigator.clipboard.writeText(markdown);
  }

  async function handleCopyMarkdown() {
    if (copyState === 'loading') return;
    setCopyState('loading');
    try {
      await copyCurrentPage();
      setCopyState('done');
    } catch {
      setCopyState('error');
    }
    window.setTimeout(() => setCopyState('idle'), 2500);
  }

  async function handleCopySnippet(kind: 'setup' | 'config') {
    await navigator.clipboard.writeText(kind === 'setup' ? MCP_SETUP : MCP_CONFIG);
    setSnippetCopied(kind);
    window.setTimeout(() => setSnippetCopied(null), 2000);
  }

  async function handleAsk(provider: Provider) {
    try {
      await copyCurrentPage();
      setAskStatus('Page Markdown copied. Paste it into the AI chat.');
    } catch {
      setAskStatus('Could not copy automatically. Use Copy as Markdown first.');
    }
    window.open(provider.url, '_blank', 'noopener,noreferrer');
  }

  const copyLabel =
    copyState === 'loading'
      ? 'Copying...'
      : copyState === 'done'
        ? 'Copied'
        : copyState === 'error'
          ? 'Copy failed'
          : 'Copy as Markdown';

  return (
    <>
      <div className={styles.toolbar}>
        <button
          className={`${styles.btn} ${copyState === 'done' ? styles.btnDone : ''} ${copyState === 'error' ? styles.btnError : ''}`}
          onClick={handleCopyMarkdown}
          disabled={copyState === 'loading'}
          title="Copy this documentation page as raw Markdown"
        >
          {copyLabel}
        </button>
        <button className={styles.btn} onClick={() => setModalOpen(true)} title="Ask AI or connect Prisma MCP">
          Ask AI
        </button>
      </div>

      {modalOpen && (
        <div className={styles.backdrop} onClick={() => setModalOpen(false)}>
          <div className={styles.modal} onClick={(event) => event.stopPropagation()}>
            <div className={styles.modalHeader}>
              <div>
                <h3 className={styles.modalTitle}>AI & Prisma MCP</h3>
                <p className={styles.modalSubtitle}>Use MCP for development. Use page Markdown for quick questions.</p>
              </div>
              <button className={styles.closeBtn} onClick={() => setModalOpen(false)} aria-label="Close">
                Close
              </button>
            </div>

            <section className={styles.modalSection}>
              <p className={styles.sectionTitle}>Prisma MCP - recommended</p>
              <p className={styles.sectionDesc}>
                Connect an MCP-compatible coding client to the public PrismaUI developer surface. The server exposes structured framework release data, API methods, guides, documentation search, and plugin scaffolding so an agent does not need to guess from stale examples.
              </p>
              <p className={styles.sectionDesc}>
                Requires Node.js 20 or newer. Build the MCP server from this public repository:
              </p>
              <div className={styles.codeWrap}>
                <pre className={styles.pre}><code>{MCP_SETUP}</code></pre>
                <button className={styles.copyCodeBtn} onClick={() => handleCopySnippet('setup')}>
                  {snippetCopied === 'setup' ? 'Copied' : 'Copy'}
                </button>
              </div>
              <p className={styles.sectionDesc}>Then point your MCP client at the built stdio server:</p>
              <div className={styles.codeWrap}>
                <pre className={styles.pre}><code>{MCP_CONFIG}</code></pre>
                <button className={styles.copyCodeBtn} onClick={() => handleCopySnippet('config')}>
                  {snippetCopied === 'config' ? 'Copied' : 'Copy'}
                </button>
              </div>
              <p className={styles.toolList}>
                Tools include <code>get_framework_release</code>, <code>get_header</code>, <code>list_api_methods</code>, <code>get_api_method</code>, <code>search_docs</code>, <code>get_guide</code>, and <code>scaffold_plugin</code>.
              </p>
              <a className={styles.guideLink} href={mcpGuidePath}>Open the full AI & MCP setup guide</a>
            </section>

            <hr className={styles.divider} />

            <section className={styles.modalSection}>
              <p className={styles.sectionTitle}>Ask AI about this page</p>
              <p className={styles.sectionDesc}>
                These buttons copy the current page as raw Markdown, then open the selected AI service. Paste the copied Markdown into the chat so the answer has the exact documentation context.
              </p>
              <div className={styles.providerRow}>
                {PROVIDERS.map((provider) => (
                  <button key={provider.label} className={styles.btnPrimary} onClick={() => handleAsk(provider)}>
                    {provider.label}
                  </button>
                ))}
              </div>
              {askStatus && <p className={styles.askStatus}>{askStatus}</p>}
            </section>
          </div>
        </div>
      )}
    </>
  );
}
