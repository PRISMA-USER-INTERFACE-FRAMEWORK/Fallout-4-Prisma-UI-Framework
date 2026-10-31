'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const renderer =
  process.argv[2] || path.join(__dirname, '../assets/views/prisma_controller_glyphs.js');
const window = {};
vm.runInNewContext(fs.readFileSync(renderer, 'utf8'), { window });

for (const ps of [false, true]) {
  for (const token of [
    'A',
    'B',
    'X',
    'Y',
    'LB',
    'RB',
    'LT',
    'RT',
    'LS',
    'RS',
    'Back',
    'DUp',
    'DDown',
    'DLeft',
    'DRight',
    'DPad',
  ]) {
    const markup = window.PrismaCG.renderKey(`gp:${token}`, ps);
    assert.match(markup, /^<img\b/);
    assert.match(markup, /\bwidth="22"/);
    assert.match(markup, /\bheight="22"/);
    assert(markup.includes(`icons/ControllerButtons/${ps ? 'P5Gamepad' : 'XGamepad'}/Default/`));
    const style = markup.match(/\bstyle="([^"]*)"/)?.[1] || '';
    assert.doesNotMatch(
      style,
      /(?:^|;)\s*(?:width|height)\s*:/i,
      'consumer CSS must control rendered dimensions'
    );
  }
}

assert.equal(window.PrismaCG.renderKey('gp:Start', false), '<span class="cg cg-pill">Menu</span>');
assert.match(window.PrismaCG.renderKey('gp:Start', true), /T_P5_Options\.png/);
assert.equal(window.PrismaCG.renderKey('Space', false), '<span class="cg cg-key">Space</span>');
assert.equal(
  window.PrismaCG.renderKey('<E&>', true),
  '<span class="cg cg-key">&lt;E&amp;&gt;</span>'
);
assert.equal(window.PrismaCG.renderKey('gp:unknown', false), '');

console.log('controller glyph renderer regression passed');
