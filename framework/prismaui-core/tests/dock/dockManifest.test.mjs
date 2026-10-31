import { test } from 'node:test';
import assert from 'node:assert/strict';
import { fileURLToPath } from 'node:url';
import { readFileSync } from 'node:fs';
import { runInThisContext } from 'node:vm';
import path from 'node:path';

// dockManifest.js ships as a classic <script> that dock.html loads, so it is exercised here the
// same way the browser loads it: run as a script that publishes itself on the global, not imported
// as a module. Same realm deliberately, so arrays it returns compare by structure against the
// test's own.
const here = path.dirname(fileURLToPath(import.meta.url));
const source = readFileSync(path.join(here, '../../shell/public/system/dockManifest.js'), 'utf8');
runInThisContext(source, { filename: 'dockManifest.js' });
const {
  modFolderFromThumb,
  groupViewsByMod,
  isSafeRelativeAsset,
  resolveImage,
  buildCard,
  PLACEHOLDER_THUMB,
  UNAUTHORED_DESCRIPTION,
} = globalThis.PrismaDockManifest;

const VIEW_ORIGIN = 'prisma://view-0123456789abcdef';
const THUMB = `${VIEW_ORIGIN}/Interface/PrismaHUD_F4/dockimage.png`;

test('modFolderFromThumb pulls the mod folder out of a view thumb path', () => {
  assert.equal(modFolderFromThumb(THUMB), 'PrismaHUD_F4');
});

test('modFolderFromThumb returns null for anything it cannot read', () => {
  assert.equal(modFolderFromThumb(''), null);
  assert.equal(modFolderFromThumb(null), null);
  assert.equal(modFolderFromThumb(undefined), null);
});

test('groupViewsByMod collapses several views of one mod into a single entry', () => {
  const views = [
    { id: 1, title: 'HUD', hidden: false, thumb: THUMB },
    { id: 2, title: 'HUD Editor', hidden: false, thumb: THUMB },
  ];
  const mods = groupViewsByMod(views);
  assert.equal(mods.length, 1);
  assert.equal(mods[0].mod, 'PrismaHUD_F4');
  assert.deepEqual(mods[0].viewIds, [1, 2]);
});

test('groupViewsByMod keeps different mods apart', () => {
  const mods = groupViewsByMod([
    { id: 1, title: 'HUD', hidden: false, thumb: THUMB },
    {
      id: 2,
      title: 'Chat',
      hidden: false,
      thumb: `${VIEW_ORIGIN}/Interface/FalloutChat/dockimage.png`,
    },
  ]);
  assert.equal(mods.length, 2);
  assert.deepEqual(mods.map((m) => m.mod).sort(), ['FalloutChat', 'PrismaHUD_F4']);
});

test('groupViewsByMod treats object property names as ordinary mod folders', () => {
  for (const mod of ['__proto__', 'constructor', 'toString']) {
    const thumb = `../Interface/${mod}/dockimage.png`;
    const mods = groupViewsByMod([
      { id: 1, title: 'Helper', hidden: true, thumb },
      { id: 2, title: 'Panel', hidden: false, thumb },
    ]);
    assert.equal(mods.length, 1, mod);
    assert.equal(mods[0].title, 'Panel', mod);
    assert.deepEqual(mods[0].viewIds, [1, 2], mod);
  }
});

// The dock is only reachable from inside the ESC menu, and plugins routinely hide their views
// while any menu is open (FalloutChat does exactly this). Filtering on "visible right now" therefore
// hid the very mods worth reading about, so the rule is "has a live view", not "is on screen".
test('groupViewsByMod keeps a mod whose windows are all hidden right now', () => {
  const mods = groupViewsByMod([
    { id: 1, title: 'Chat', hidden: true, thumb: THUMB },
    { id: 2, title: 'Chat Settings', hidden: true, thumb: THUMB },
  ]);
  assert.equal(mods.length, 1);
  assert.deepEqual(mods[0].viewIds, [1, 2]);
});

test('groupViewsByMod lists nothing when no views exist at all', () => {
  assert.deepEqual(groupViewsByMod([]), []);
});

test('groupViewsByMod counts every live view of a mod, visible or not', () => {
  const mods = groupViewsByMod([
    { id: 1, title: 'HUD', hidden: true, thumb: THUMB },
    { id: 2, title: 'HUD Editor', hidden: false, thumb: THUMB },
  ]);
  assert.equal(mods.length, 1);
  assert.deepEqual(mods[0].viewIds, [1, 2]);
});

test('groupViewsByMod titles a mod from its first visible view when it has one', () => {
  const mods = groupViewsByMod([
    { id: 1, title: 'hidden one', hidden: true, thumb: THUMB },
    { id: 2, title: 'visible one', hidden: false, thumb: THUMB },
  ]);
  assert.equal(mods[0].title, 'visible one');
});

test('isSafeRelativeAsset accepts a plain relative file inside the mod folder', () => {
  assert.equal(isSafeRelativeAsset('dockimage.png'), true);
  assert.equal(isSafeRelativeAsset('art/banner.png'), true);
});

test('isSafeRelativeAsset rejects anything with a scheme', () => {
  assert.equal(isSafeRelativeAsset('https://example.com/a.png'), false);
  assert.equal(isSafeRelativeAsset('prisma://views/x/a.png'), false);
  assert.equal(isSafeRelativeAsset('//example.com/a.png'), false);
  assert.equal(isSafeRelativeAsset('data:image/png;base64,AAAA'), false);
});

test('isSafeRelativeAsset rejects escaping the mod folder', () => {
  assert.equal(isSafeRelativeAsset('../other/a.png'), false);
  assert.equal(isSafeRelativeAsset('art/../../a.png'), false);
  assert.equal(isSafeRelativeAsset('/abs/a.png'), false);
  assert.equal(isSafeRelativeAsset('\\abs\\a.png'), false);
  assert.equal(isSafeRelativeAsset('C:/a.png'), false);
});

test('isSafeRelativeAsset rejects empty and non-strings', () => {
  assert.equal(isSafeRelativeAsset(''), false);
  assert.equal(isSafeRelativeAsset('   '), false);
  assert.equal(isSafeRelativeAsset(null), false);
  assert.equal(isSafeRelativeAsset(42), false);
});

test("resolveImage puts a safe relative path inside the mod's own folder", () => {
  assert.equal(
    resolveImage(THUMB, 'art/banner.png'),
    `${VIEW_ORIGIN}/Interface/PrismaHUD_F4/art/banner.png`
  );
});

test('resolveImage falls back to the convention image when the manifest has none', () => {
  assert.equal(resolveImage(THUMB, undefined), THUMB);
});

test('resolveImage refuses a URL and falls back rather than emitting it', () => {
  assert.equal(resolveImage(THUMB, 'https://example.com/a.png'), THUMB);
  assert.equal(resolveImage(THUMB, '../../escape.png'), THUMB);
});

test('buildCard with no manifest says the author has not set a page up', () => {
  const card = buildCard({ mod: 'PrismaHUD_F4', title: 'HUD', thumb: THUMB }, null);
  assert.equal(card.name, 'HUD');
  assert.equal(card.description, UNAUTHORED_DESCRIPTION);
  assert.equal(card.authored, false);
  assert.equal(card.image, THUMB);
  assert.equal(card.nexus, null);
});

test('buildCard uses the manifest when one is present', () => {
  const card = buildCard(
    { mod: 'PrismaHUD_F4', title: 'HUD', thumb: THUMB },
    {
      name: 'Prisma HUD',
      description: 'A replacement HUD.',
      image: 'art/banner.png',
      nexus: 'https://www.nexusmods.com/fallout4/mods/12345',
    }
  );
  assert.equal(card.name, 'Prisma HUD');
  assert.equal(card.description, 'A replacement HUD.');
  assert.equal(card.image, `${VIEW_ORIGIN}/Interface/PrismaHUD_F4/art/banner.png`);
  assert.equal(card.nexus, 'https://www.nexusmods.com/fallout4/mods/12345');
  assert.equal(card.authored, true);
});

test('buildCard treats a manifest with no usable fields as unauthored', () => {
  const card = buildCard({ mod: 'PrismaHUD_F4', title: 'HUD', thumb: THUMB }, {});
  assert.equal(card.authored, false);
  assert.equal(card.description, UNAUTHORED_DESCRIPTION);
});

test('buildCard leaves the external link empty when nexus is missing or blank', () => {
  assert.equal(buildCard({ mod: 'M', title: 'T', thumb: THUMB }, { nexus: '   ' }).nexus, null);
  assert.equal(buildCard({ mod: 'M', title: 'T', thumb: THUMB }, { nexus: null }).nexus, null);
});

test('buildCard refuses a non-http nexus link rather than handing it to the shell', () => {
  const card = buildCard(
    { mod: 'M', title: 'T', thumb: THUMB },
    { nexus: 'file:///C:/Windows/System32/cmd.exe' }
  );
  assert.equal(card.nexus, null);
});

test('buildCard refuses an HTTPS link outside Nexus Mods', () => {
  const card = buildCard(
    { mod: 'M', title: 'T', thumb: THUMB },
    { nexus: 'https://example.com/steal' }
  );
  assert.equal(card.nexus, null);
});

test('buildCard survives a manifest that is not an object', () => {
  for (const junk of ['nope', 7, [], true]) {
    const card = buildCard({ mod: 'M', title: 'T', thumb: THUMB }, junk);
    assert.equal(card.authored, false);
    assert.equal(card.description, UNAUTHORED_DESCRIPTION);
  }
});

test('buildCard ignores non-string manifest fields instead of rendering them', () => {
  const card = buildCard(
    { mod: 'M', title: 'T', thumb: THUMB },
    { name: 5, description: {}, nexus: [] }
  );
  assert.equal(card.name, 'T');
  assert.equal(card.description, UNAUTHORED_DESCRIPTION);
  assert.equal(card.nexus, null);
});

test('buildCard uses the placeholder when the mod has no thumb at all', () => {
  const card = buildCard({ mod: 'M', title: 'T', thumb: null }, null);
  assert.equal(card.image, PLACEHOLDER_THUMB);
});
