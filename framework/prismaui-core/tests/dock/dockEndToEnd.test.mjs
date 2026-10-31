import { test } from 'node:test';
import assert from 'node:assert/strict';
import { fileURLToPath } from 'node:url';
import { readFileSync } from 'node:fs';
import { runInThisContext } from 'node:vm';
import path from 'node:path';

// Drives the whole dock path with real files: the Host's payload shape in, a real dock.json read
// off disk, a real card out. Everything except the DOM render, which needs the game.
const here = path.dirname(fileURLToPath(import.meta.url));
const shellSystem = process.env.PRISMA_DOCK_PACKAGE_ROOT
  ? path.join(process.env.PRISMA_DOCK_PACKAGE_ROOT, 'system')
  : path.join(here, '../../shell/public/system');
runInThisContext(readFileSync(path.join(shellSystem, 'dockManifest.js'), 'utf8'), {
  filename: 'dockManifest.js',
});
const DM = globalThis.PrismaDockManifest;

// Exactly what the host's BuildDockPayload emits, including the thumb convention.
function hostPayload(views) {
  return { welcome: false, views };
}
function hostThumb(viewUrl) {
  const slash = viewUrl.lastIndexOf('/');
  return slash === -1 ? '' : viewUrl.slice(0, slash) + '/dockimage.png';
}

// The same transform dock.html applies: thumb -> dock.json URL.
function dockJsonUrl(m) {
  return m.thumb ? m.thumb.replace(/dockimage\.png$/i, 'dock.json') : null;
}

const FIXTURES = path.join(here, 'fixtures');

// Stands in for the Host's prisma:// scheme handler. Shipping views use an isolated view-<hash>
// origin; legacy prisma://views remains accepted for compatibility.
function fetchFixture(prismaUrl) {
  const rest = prismaUrl.replace(/^prisma:\/\/(?:views|view-[a-f0-9]+)\//, '');
  try {
    return JSON.parse(readFileSync(path.join(FIXTURES, rest), 'utf8'));
  } catch {
    return null;
  }
}

test('end to end: a mod that ships a real dock.json renders its own page', () => {
  const viewUrl = 'prisma://view-a11ce00000000001/Interface/AuthoredMod/panel.html';
  const payload = hostPayload([
    { id: 7, title: 'panel', hidden: false, thumb: hostThumb(viewUrl) },
  ]);

  const mods = DM.groupViewsByMod(payload.views);
  assert.equal(mods.length, 1, 'one tile for the one open mod');
  assert.equal(mods[0].mod, 'AuthoredMod');

  const manifest = fetchFixture(dockJsonUrl(mods[0]));
  assert.notEqual(manifest, null, 'the fixture dock.json was actually read');

  const card = DM.buildCard(mods[0], manifest);
  assert.equal(card.authored, true);
  assert.equal(card.name, 'Authored Mod');
  assert.equal(card.description, 'A mod that ships its own dock page.');
  assert.equal(card.image, 'prisma://view-a11ce00000000001/Interface/AuthoredMod/art/banner.png');
  assert.equal(card.nexus, 'https://www.nexusmods.com/fallout4/mods/12345');
});

test('end to end: a mod with no dock.json gets the honest placeholder page', () => {
  const viewUrl = 'prisma://view-ba4e000000000002/Interface/BareMod/panel.html';
  const mods = DM.groupViewsByMod(
    hostPayload([{ id: 9, title: 'panel', hidden: false, thumb: hostThumb(viewUrl) }]).views
  );
  const manifest = fetchFixture(dockJsonUrl(mods[0]));
  assert.equal(manifest, null, 'there is deliberately no dock.json for this one');

  const card = DM.buildCard(mods[0], manifest);
  assert.equal(card.authored, false);
  assert.equal(card.name, 'panel');
  assert.equal(card.description, DM.UNAUTHORED_DESCRIPTION);
  assert.equal(card.nexus, null);
  assert.equal(card.image, 'prisma://view-ba4e000000000002/Interface/BareMod/dockimage.png');
});

test('end to end: a dock.json pointing at a remote image falls back instead of emitting the URL', () => {
  const viewUrl = 'prisma://view-e000000000000003/Interface/RemoteImageMod/panel.html';
  const mods = DM.groupViewsByMod(
    hostPayload([{ id: 11, title: 'panel', hidden: false, thumb: hostThumb(viewUrl) }]).views
  );
  const card = DM.buildCard(mods[0], fetchFixture(dockJsonUrl(mods[0])));
  assert.equal(card.image, 'prisma://view-e000000000000003/Interface/RemoteImageMod/dockimage.png');
  assert.match(card.name, /Remote/);
});

test('end to end: malformed dock.json degrades to the placeholder page, it does not throw', () => {
  const viewUrl = 'prisma://view-b000000000000004/Interface/BrokenMod/panel.html';
  const mods = DM.groupViewsByMod(
    hostPayload([{ id: 13, title: 'panel', hidden: false, thumb: hostThumb(viewUrl) }]).views
  );
  const manifest = fetchFixture(dockJsonUrl(mods[0]));
  assert.equal(manifest, null, 'unparseable JSON reads as no manifest');
  const card = DM.buildCard(mods[0], manifest);
  assert.equal(card.authored, false);
  assert.equal(card.description, DM.UNAUTHORED_DESCRIPTION);
});

test('end to end: two open windows of one mod produce a single tile', () => {
  const thumb = hostThumb('prisma://view-a11ce00000000001/Interface/AuthoredMod/panel.html');
  const mods = DM.groupViewsByMod(
    hostPayload([
      { id: 1, title: 'panel', hidden: false, thumb },
      { id: 2, title: 'settings', hidden: false, thumb },
    ]).views
  );
  assert.equal(mods.length, 1);
  assert.deepEqual(mods[0].viewIds, [1, 2]);
  assert.equal(DM.buildCard(mods[0], fetchFixture(dockJsonUrl(mods[0]))).name, 'Authored Mod');
});

// Reproduces what actually happened in game: chat creates its view hidden and re-hides whenever a
// menu opens, and the dock lives inside the ESC menu, so chat was hidden every single time the dock
// could be looked at. Under a visibility filter it could never be listed.
test('end to end: a mod that hides its view while menus are open is still listed', () => {
  const thumb = hostThumb('prisma://view-a11ce00000000001/Interface/AuthoredMod/panel.html');
  const mods = DM.groupViewsByMod(
    hostPayload([{ id: 1, title: 'panel', hidden: true, thumb }]).views
  );
  assert.equal(mods.length, 1);
  assert.equal(DM.buildCard(mods[0], fetchFixture(dockJsonUrl(mods[0]))).name, 'Authored Mod');
});
