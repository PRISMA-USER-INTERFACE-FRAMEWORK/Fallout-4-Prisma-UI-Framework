// Shared controller button-art renderer, used by every PrismaHUD view that prints a button prompt
// (interaction, quick-loot, message box). Self-contained -- draws the art itself (colored Xbox face
// circles, PlayStation shape SVGs, labelled pills), no external asset files to license or ship.
//
// C++ (ControlLabels) sends a gamepad prompt as the marker "gp:<canonical>" (e.g. "gp:A", "gp:LB",
// "gp:DPad"); a keyboard prompt is sent as plain text (e.g. "E", "Space"). CG.renderKey turns either
// into display HTML. CG.chip renders a known canonical id directly (message-box glyph decoding).
(function () {
  // Real button art: Julio Cacko "Free Input Prompts" (https://juliocacko.itch.io/free-input-prompts),
  // Xbox (XGamepad) + PlayStation 5 (P5Gamepad) Default sets, deployed under views/icons/ControllerButtons.
  const XB = 'icons/ControllerButtons/XGamepad/Default/';
  const P5 = 'icons/ControllerButtons/P5Gamepad/Default/';

  // canonical -> [xbox file, playstation file]. "" = no art for that style (renderKey falls back to pill).
  const ICON = {
    A:      [XB + 'T_X_A_Color.png',            P5 + 'T_P5_Cross_Color.png'],
    B:      [XB + 'T_X_B_Color.png',            P5 + 'T_P5_Circle_Color.png'],
    X:      [XB + 'T_X_X_Color.png',            P5 + 'T_P5_Square_Color.png'],
    Y:      [XB + 'T_X_Y_Color.png',            P5 + 'T_P5_Triangle_Color.png'],
    LB:     [XB + 'T_X_LB.png',                 P5 + 'T_P5_L1.png'],
    RB:     [XB + 'T_X_RB.png',                 P5 + 'T_P5_R1.png'],
    LT:     [XB + 'T_X_LT.png',                 P5 + 'T_P5_L2.png'],
    RT:     [XB + 'T_X_RT.png',                 P5 + 'T_P5_R2.png'],
    LS:     [XB + 'T_X_Left_Stick_Click.png',   P5 + 'T_P5_L3.png'],
    RS:     [XB + 'T_X_Right_Stick_Click.png',  P5 + 'T_P5_R3.png'],
    Start:  ['',                                P5 + 'T_P5_Options.png'],
    Back:   [XB + 'T_X_Share.png',              P5 + 'T_P5_Share.png'],
    DUp:    [XB + 'T_X_Dpad_Up.png',            P5 + 'T_P5_Dpad_UP.png'],
    DDown:  [XB + 'T_X_Dpad_Down.png',          P5 + 'T_P5_Dpad_Down.png'],
    DLeft:  [XB + 'T_X_Dpad_Left.png',          P5 + 'T_P5_Dpad_Left.png'],
    DRight: [XB + 'T_X_Dpad_Right.png',         P5 + 'T_P5_Dpad_Right.png'],
    DPad:   [XB + 'T_X_Dpad.png',               P5 + 'T_P5_Dpad.png'],
  };

  // Pill fallback text (used only when a style has no art file for a canonical id).
  const PILL = {
    LB: ['LB', 'L1'], RB: ['RB', 'R1'], LT: ['LT', 'L2'], RT: ['RT', 'R2'],
    LS: ['LS', 'L3'], RS: ['RS', 'R3'], Start: ['Menu', 'Options'], Back: ['View', 'Share'],
    DUp: ['↑', '↑'], DDown: ['↓', '↓'], DLeft: ['←', '←'], DRight: ['→', '→'], DPad: ['✚', '✚'],
    A: ['A', '✕'], B: ['B', '○'], X: ['X', '□'], Y: ['Y', '△'],
  };

  // Verified glyph char -> canonical (decompile table; uppercase T/U/V/W are Bethesda's baked d-pad).
  const CHAR = {
    A: 'A', B: 'B', C: 'X', D: 'Y', E: 'Back', F: 'LS', G: 'LB', H: 'L3', I: 'LT',
    K: 'RS', L: 'RB', M: 'R3', N: 'RT', O: 'Start',
    a: 'A', b: 'Y', c: 'X', d: 'B', f: 'L3', g: 'LB', i: 'LS', j: 'LT', l: 'R3',
    m: 'RB', n: 'RS', o: 'RT', p: 'Start', z: 'Back',
    s: 'DPad', t: 'DLeft', u: 'DRight', v: 'DDown', w: 'DUp',
    S: 'DPad', T: 'DLeft', U: 'DRight', V: 'DDown', W: 'DUp',
  };

  function esc(s) { return String(s == null ? '' : s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;'); }

  function chip(canon, ps) {
    const icon = ICON[canon];
    const file = icon ? icon[ps ? 1 : 0] : '';
    if (file) return `<img class="cg cg-img" src="${file}" alt="">`;
    const p = PILL[canon];
    if (p) return `<span class="cg cg-pill">${esc(ps ? p[1] : p[0])}</span>`;
    return '';
  }

  // label = "gp:<canonical>" for a gamepad button, or plain keyboard text.
  function renderKey(label, ps) {
    if (typeof label === 'string' && label.startsWith('gp:')) return chip(label.slice(3), ps) || '';
    return `<span class="cg cg-key">${esc(label)}</span>`;
  }

  window.PrismaCG = { chip, renderKey, CHAR };
  window.CG = window.PrismaCG;   // back-compat alias
})();
