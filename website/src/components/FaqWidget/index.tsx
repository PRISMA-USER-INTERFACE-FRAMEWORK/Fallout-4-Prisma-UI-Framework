import React, { useMemo, useState } from 'react';
import Link from '@docusaurus/Link';
import useBaseUrl from '@docusaurus/useBaseUrl';
import { FAQ_DATA, type FaqEntry } from '@site/src/data/faqData';
import styles from './styles.module.css';

const DISCORD_URL = 'https://discord.com/invite/bawdketrFX';

function score(entry: FaqEntry, queryWords: string[]): number {
  if (queryWords.length === 0) return 0;
  const question = entry.question.toLowerCase();
  const answer = entry.answer.toLowerCase();
  let total = 0;
  for (const word of queryWords) {
    if (question.includes(word)) total += 3;
    if (entry.keywords.some((k) => k.includes(word) || word.includes(k))) total += 2;
    if (answer.includes(word)) total += 1;
  }
  return total;
}

function FaqAnswerLink({ entry }: { entry: FaqEntry }): JSX.Element | null {
  const href = useBaseUrl(entry.link ?? '#');
  if (!entry.link) return null;
  return (
    <Link className={styles.answerLink} to={href}>
      Read more: {entry.linkLabel} →
    </Link>
  );
}

export default function FaqWidget(): JSX.Element {
  const [open, setOpen] = useState(false);
  const [query, setQuery] = useState('');
  const [openEntry, setOpenEntry] = useState<string | null>(null);

  const results = useMemo(() => {
    const words = query
      .toLowerCase()
      .split(/\s+/)
      .map((w) => w.trim())
      .filter((w) => w.length > 1);

    if (words.length === 0) return FAQ_DATA;

    return FAQ_DATA.map((entry) => ({ entry, s: score(entry, words) }))
      .filter((r) => r.s > 0)
      .sort((a, b) => b.s - a.s)
      .map((r) => r.entry);
  }, [query]);

  function toggleEntry(id: string) {
    setOpenEntry((cur) => (cur === id ? null : id));
  }

  return (
    <>
      <button
        className={styles.launcher}
        onClick={() => setOpen((v) => !v)}
        aria-label={open ? 'Close FAQ' : 'Open FAQ'}
        title="Questions about PrismaUI?"
      >
        {open ? '✕' : '?'}
      </button>

      {open && (
        <div className={styles.panel}>
          <div className={styles.panelHeader}>
            <span className={styles.panelTitle}>PrismaUI FAQ</span>
            <button className={styles.closeBtn} onClick={() => setOpen(false)} aria-label="Close">
              ✕
            </button>
          </div>

          <input
            className={styles.searchInput}
            type="text"
            placeholder="Ask a question, e.g. &quot;UI not showing&quot;"
            value={query}
            onChange={(e) => setQuery(e.target.value)}
            autoFocus
          />

          <div className={styles.results}>
            {results.length === 0 ? (
              <div className={styles.empty}>
                <p>No matching answers yet.</p>
                <a className={styles.emptyLink} href={DISCORD_URL} target="_blank" rel="noopener noreferrer">
                  Ask on Discord →
                </a>
              </div>
            ) : (
              results.map((entry) => (
                <div key={entry.id} className={styles.entry}>
                  <button className={styles.question} onClick={() => toggleEntry(entry.id)}>
                    <span>{entry.question}</span>
                    <span className={styles.chevron}>{openEntry === entry.id ? '−' : '+'}</span>
                  </button>
                  {openEntry === entry.id && (
                    <div className={styles.answer}>
                      <p>{entry.answer}</p>
                      <FaqAnswerLink entry={entry} />
                    </div>
                  )}
                </div>
              ))
            )}
          </div>

          <div className={styles.panelFooter}>
            Still stuck?{' '}
            <a href={DISCORD_URL} target="_blank" rel="noopener noreferrer">
              Ask on Discord
            </a>
          </div>
        </div>
      )}
    </>
  );
}
