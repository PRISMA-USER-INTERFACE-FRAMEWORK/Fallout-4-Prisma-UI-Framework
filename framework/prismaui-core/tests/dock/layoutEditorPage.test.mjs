import { test } from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { existsSync, mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const harness = pathToFileURL(path.join(here, 'fixtures/layoutEditorHarness.html')).href;

function findBrowser() {
  if (process.env.PRISMA_TEST_BROWSER) return process.env.PRISMA_TEST_BROWSER;
  const candidates = [];
  if (process.platform === 'win32') {
    const roots = [process.env.ProgramFiles, process.env['ProgramFiles(x86)'], process.env.LOCALAPPDATA];
    for (const rel of ['Google/Chrome/Application/chrome.exe', 'Microsoft/Edge/Application/msedge.exe']) {
      for (const root of roots) if (root) candidates.push(path.join(root, rel));
    }
  } else if (process.platform === 'darwin') {
    candidates.push('/Applications/Google Chrome.app/Contents/MacOS/Google Chrome');
  }
  const names = ['google-chrome', 'google-chrome-stable', 'chromium', 'chromium-browser', 'microsoft-edge', 'microsoft-edge-stable'];
  for (const dir of (process.env.PATH || '').split(path.delimiter)) {
    for (const name of names) candidates.push(path.join(dir, name));
  }
  return candidates.find((file) => existsSync(file)) || null;
}

function runHarness(browser) {
  const profile = mkdtempSync(path.join(tmpdir(), 'prisma-layout-'));
  const packaged = process.env.PRISMA_DOCK_PACKAGE_ROOT;
  const url = harness + (packaged ? `#${pathToFileURL(path.join(packaged, 'system')).href}/` : '');
  try {
    const run = spawnSync(
      browser,
      [
        '--headless=new',
        '--disable-gpu',
        '--allow-file-access-from-files',
        '--no-sandbox',
        '--no-first-run',
        '--disable-extensions',
        `--user-data-dir=${profile}`,
        '--disable-crash-reporter',
        '--dump-dom',
        url,
      ],
      { encoding: 'utf8', timeout: 120000, maxBuffer: 64 * 1024 * 1024 },
    );
    if (run.error) throw run.error;
    const match = /<pre id="result">([A-Za-z0-9+/=]+)<\/pre>/.exec(run.stdout);
    if (!match) {
      throw new Error(
        `browser wrote no result (exit ${run.status})\nstderr: ${String(run.stderr).slice(-2000)}\nstdout: ${String(run.stdout).slice(0, 2000)}`,
      );
    }
    return JSON.parse(Buffer.from(match[1], 'base64').toString('utf8'));
  } finally {
    try {
      rmSync(profile, { recursive: true, force: true, maxRetries: 5 });
    } catch {}
  }
}

const browser = findBrowser();
if (!browser) {
  const reason = 'no Chrome or Edge found; set PRISMA_TEST_BROWSER to a browser executable';
  if (process.env.CI) {
    test('layout editor page test has a browser to run in (regression: CI silently skips the page test)', () => {
      assert.fail(reason);
    });
  } else {
    test('layout editor page test', { skip: reason }, () => {});
  }
} else {
  let result = null;
  let failure = null;
  try {
    result = runHarness(browser);
  } catch (err) {
    failure = err;
  }
  if (failure) {
    test('layout editor harness runs to a result in headless Chrome or Edge (regression: harness cannot load the real page)', () => {
      throw failure;
    });
  } else {
    test('layout editor scenario ran every step (regression: harness stopped early and later controls went untested)', () => {
      assert.equal(result.fatal, null, String(result.fatal));
      assert.ok(result.steps.length === 25, `only ${result.steps.length} steps ran`);
    });
    for (const step of result.steps) {
      test(step.name, () => {
        assert.ok(step.ok, step.detail);
      });
    }
  }
}
