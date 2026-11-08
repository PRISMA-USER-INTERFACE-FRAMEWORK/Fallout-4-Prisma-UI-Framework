import { test } from 'node:test';
import assert from 'node:assert/strict';
import { fileURLToPath } from 'node:url';
import { readFileSync } from 'node:fs';
import { runInThisContext } from 'node:vm';
import path from 'node:path';

const here = path.dirname(fileURLToPath(import.meta.url));
const publicSystem = path.join(here, '../../shell/public/system');
runInThisContext(readFileSync(path.join(publicSystem, 'layoutCore.js'), 'utf8'), {
  filename: 'layoutCore.js',
});
const core = globalThis.PrismaLayoutCore;

test('moves snap to the grid and keep part of the view on the stage', () => {
  const moved = core.moveRect({ x: 100, y: 100, w: 400, h: 300 }, 13, 7, 10);
  assert.deepEqual(moved, { x: 110, y: 110, w: 400, h: 300 });
  const flung = core.moveRect({ x: 0, y: 0, w: 400, h: 300 }, 99999, -99999, 0);
  assert.equal(flung.x, core.STAGE_W - core.MIN_SIZE);
  assert.equal(flung.y, core.MIN_SIZE - 300);
});

test('resize handles move only their own edges and never go below the minimum size', () => {
  const rect = { x: 100, y: 100, w: 400, h: 300 };
  assert.deepEqual(core.resizeRect(rect, 'e', 50, 999, 0), { x: 100, y: 100, w: 450, h: 300 });
  assert.deepEqual(core.resizeRect(rect, 'nw', 50, 50, 0), { x: 150, y: 150, w: 350, h: 250 });
  const crushed = core.resizeRect(rect, 'w', 9999, 0, 0);
  assert.equal(crushed.w, core.MIN_SIZE);
  assert.equal(crushed.x + crushed.w, 500);
  assert.equal(core.resizeRect(rect, 'se', 99999, 99999, 0).w, core.STAGE_W * 2);
});

test('snap cycles through its steps and wraps', () => {
  assert.deepEqual(core.SNAP_STEPS, [0, 5, 10, 20]);
  assert.equal(core.cycleSnap(0), 5);
  assert.equal(core.cycleSnap(20), 0);
  assert.equal(core.cycleSnap(7), 0);
});

test('selection walks the view list in both directions and wraps', () => {
  assert.equal(core.neighbour([4, 5, 6], 6, 1), 4);
  assert.equal(core.neighbour([4, 5, 6], 4, -1), 6);
  assert.equal(core.neighbour([4, 5, 6], 99, 1), 4);
  assert.equal(core.neighbour([], null, 1), null);
});

test('undo and redo restore the exact earlier state and a new edit drops the redo branch', () => {
  const history = core.createHistory(100);
  const a = { rect: { x: 0, y: 0, w: 100, h: 100 }, hidden: false };
  const b = { rect: { x: 10, y: 0, w: 100, h: 100 }, hidden: false };
  const c = { rect: { x: 10, y: 0, w: 100, h: 100 }, hidden: true };
  history.push({ id: 3, key: null, before: a, after: b }, 0);
  history.push({ id: 3, key: null, before: b, after: c }, 1);
  assert.deepEqual(history.undo(), { id: 3, state: b });
  assert.deepEqual(history.undo(), { id: 3, state: a });
  assert.equal(history.undo(), null);
  assert.deepEqual(history.redo(), { id: 3, state: b });
  history.push({ id: 3, key: null, before: b, after: a }, 2);
  assert.equal(history.canRedo(), false);
});

test('rapid keyboard nudges of one view collapse into a single undo step', () => {
  const history = core.createHistory(100);
  const at = (x) => ({ rect: { x, y: 0, w: 100, h: 100 }, hidden: false });
  history.push({ id: 1, key: 'move-key', before: at(0), after: at(10) }, 1000);
  history.push({ id: 1, key: 'move-key', before: at(10), after: at(20) }, 1100);
  history.push({ id: 2, key: 'move-key', before: at(0), after: at(10) }, 1150);
  history.push({ id: 1, key: 'move-key', before: at(20), after: at(30) }, 2000);
  assert.deepEqual(history.undo(), { id: 1, state: at(20) });
  assert.deepEqual(history.undo(), { id: 2, state: at(0) });
  assert.deepEqual(history.undo(), { id: 1, state: at(0) });
  assert.equal(history.canUndo(), false);
});

test('history drops its oldest entry past the limit', () => {
  const history = core.createHistory(2);
  for (let i = 0; i < 3; i++) {
    history.push({ id: i, key: null, before: i, after: i + 1 }, i);
  }
  assert.equal(history.undo().id, 2);
  assert.equal(history.undo().id, 1);
  assert.equal(history.undo(), null);
});

test('native layout payloads parse and bad ones are rejected', () => {
  const payload = JSON.stringify({
    snap: 20,
    grid: true,
    views: [
      { id: 7, title: 'Hud', owner: 'Mod.dll', registered: true, hidden: true, rect: [1, 2, 300, 200], default: [0, 0, 640, 360] },
      { id: 8, rect: [0, 0, 1920, 1080] },
      { id: 9, rect: [0, 0, 'x', 4] },
      { id: 10, rect: [0, 0, 5] },
    ],
  });
  const parsed = core.parseLayout(payload);
  assert.equal(parsed.snap, 20);
  assert.equal(parsed.grid, true);
  assert.deepEqual(
    parsed.views.map((v) => v.id),
    [7, 8],
  );
  assert.equal(parsed.views[0].hidden, true);
  assert.deepEqual(parsed.views[0].defaultRect, { x: 0, y: 0, w: 640, h: 360 });
  assert.equal(parsed.views[1].title, 'View 8');
  assert.deepEqual(parsed.views[1].defaultRect, { x: 0, y: 0, w: core.STAGE_W, h: core.STAGE_H });
  assert.equal(core.parseLayout(JSON.stringify({ snap: 3, views: [] })).snap, 10);
  assert.equal(core.parseLayout('{not json'), null);
  assert.equal(core.parseLayout('{"views":4}'), null);
});

test('bridge messages use the channel and payload form the native handler splits', () => {
  assert.equal(core.message('save', ''), 'save\n');
  assert.equal(core.rectMessage(5, { x: 1, y: 2, w: 3, h: 4 }), 'rect\n5,1,2,3,4');
});

test('the editor page only posts channels the native editor bridge handles', () => {
  const page = readFileSync(path.join(publicSystem, 'layout.html'), 'utf8');
  const native = readFileSync(path.join(here, '../../src/API/LayoutAPI.cpp'), 'utf8');
  const posted = new Set([...page.matchAll(/native\('(\w+)'/g)].map((m) => m[1]));
  assert.ok(posted.size >= 8, 'the page should post its channels through native()');
  for (const channel of posted) {
    assert.ok(native.includes(`"${channel}"`), `native editor handler is missing channel ${channel}`);
  }
});

test('the dock edit button posts the channel the dock bridge handles', () => {
  const dock = readFileSync(path.join(publicSystem, 'dock.html'), 'utf8');
  const bridge = readFileSync(path.join(here, '../../src/PrismaUI/DockBridge.cpp'), 'utf8');
  assert.match(dock, /native\('layoutEdit', '1'\)/);
  assert.ok(bridge.includes('"layoutEdit"'));
});
