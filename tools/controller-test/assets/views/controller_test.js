(() => {
  const lines = [];
  const maxLines = 160;
  const $ = id => document.getElementById(id);
  const stamp = () => new Date().toISOString().slice(11, 23);
  const write = (kind, value) => {
    const line = `${stamp()} ${kind} ${typeof value === 'string' ? value : JSON.stringify(value)}`;
    lines.push(line);
    if (lines.length > maxLines) lines.shift();
    $('events').textContent = lines.join('\n');
    $('count').textContent = lines.length;
    console.info(`[ControllerProbe] ${line}`);
  };
  const send = (name, value = '') => {
    const handler = window[name];
    if (typeof handler === 'function') {
      handler(typeof value === 'string' ? value : JSON.stringify(value));
      return;
    }
    write('error', `missing JS handler window.${name}`);
  };
  const set = (id, value) => { $(id).textContent = value; };
  const glyphNames = ['A', 'B', 'DUp', 'DDown', 'DLeft', 'DRight', 'LB', 'RB', 'LT', 'RT'];
  const renderGlyphs = ps => {
    if (!window.PrismaCG) { $('glyphs').textContent = 'shared glyph renderer unavailable'; return; }
    $('glyphs').innerHTML = glyphNames.map(name =>
      `<span class="glyph" data-glyph="${name}">${window.PrismaCG.renderKey(`gp:${name}`, ps)} ${name}</span>`).join('');
  };
  const updateControllerState = state => {
    if (typeof state.gamepad === 'boolean') set('device', state.gamepad ? 'gamepad' : 'keyboard');
    if (typeof state.style === 'number') {
      set('style', state.style === 1 ? 'playstation' : 'xbox');
      renderGlyphs(state.style === 1);
    }
  };
  const setControllerState = value => {
    try {
      updateControllerState(JSON.parse(value));
    } catch (_) { write('error', `invalid controller state ${value}`); }
  };
  window.setControllerState = setControllerState;
  window.setFocusState = value => set('focus', value === 'true' ? 'focused' : 'released');
  window.setNativeState = value => set('native-state', value === 'true' ? 'on' : 'off');

  ['keydown', 'keyup'].forEach(type => document.addEventListener(type, event => {
    write(type, { key: event.key, code: event.code, repeat: event.repeat, defaultPrevented: event.defaultPrevented });
  }, true));

  ['mousedown', 'mouseup', 'click'].forEach(type => document.addEventListener(type, event => {
    const target = event.target || {};
    write(type, {
      target: (typeof target.id === 'string' && target.id) || target.tagName || 'unknown',
      tag: target.tagName || '',
      id: target.id || '',
      x: event.clientX,
      y: event.clientY,
      button: event.button,
      detail: event.detail,
    });
  }, true));

  let pressSeq = 0;
  window.addEventListener('prisma-controller-action', event => {
    const detail = event.detail || {};
    if (detail.state === 'pressed') ++pressSeq;
    write('action', Object.assign({}, detail, { press: pressSeq }));
    if (detail.state === 'pressed' && String(detail.action || '').startsWith('probe.osk.') && window.PrismaOSK && !window.PrismaOSK.isOpen()) {
      send('controller_osk', 'off');
      write('osk', 'released stale keyboard bindings');
    }
  });
  window.addEventListener('prisma-input-device-change', event => {
    const detail = event.detail || {};
    updateControllerState(detail);
    write('device', detail);
  });
  let lastWindowEvent = '';
  ['focus', 'blur', 'pageshow', 'pagehide'].forEach(type => window.addEventListener(type, () => {
    if (lastWindowEvent === type) return;
    lastWindowEvent = type;
    write(type, '');
  }));
  document.addEventListener('visibilitychange', () => write('visibilitychange', document.visibilityState));

  $('release').onclick = () => { send('controller_release_focus'); write('request', 'release focus'); set('focus', 'released'); };
  $('native').onclick = () => { send('controller_toggle_native'); write('request', 'toggle native gamepad'); };
  $('reload').onclick = () => { send('controller_reload'); write('request', 'reload document'); };
  $('recreate').onclick = () => { send('controller_recreate'); write('request', 'destroy and recreate'); };
  $('close').onclick = () => { send('controller_close'); write('request', 'close'); };

  const osk = window.PrismaOSK;
  const oskHost = $('osk-host');
  const oskTargets = { 'osk-open-input': 'osk-input', 'osk-open-area': 'osk-area', 'osk-open-rich': 'osk-rich' };
  if (osk) {
    osk.bindHost(oskHost);
    let oskOpen = false;
    const syncOsk = () => {
      if (osk.isOpen() === oskOpen) return;
      oskOpen = osk.isOpen();
      const target = osk.getTarget();
      set('osk-state', oskOpen ? `open on #${target && target.id}` : 'closed');
      send('controller_osk', oskOpen ? 'on' : 'off');
      write('osk', oskOpen ? `open #${target && target.id}` : 'closed');
    };
    new MutationObserver(syncOsk).observe(oskHost, { attributes: true, attributeFilter: ['hidden'] });
    Object.keys(oskTargets).forEach(id => {
      $(id).onclick = () => {
        const target = $(oskTargets[id]);
        write('request', `open keyboard #${target.id} opened=${osk.open(target)}`);
        syncOsk();
      };
    });
    Object.values(oskTargets).forEach(id => $(id).addEventListener('input', event => {
      write('input', { target: event.target.id, bubbles: event.bubbles, value: event.target.value !== undefined ? event.target.value : event.target.textContent });
    }));
  } else {
    set('osk-state', 'shared prisma_osk.js unavailable');
    write('error', 'shared prisma_osk.js unavailable');
  }

  let previous = '';
  let lastPoll = 0;
  const quantize = value => Math.abs(value) < 0.05 ? 0 : +value.toFixed(1);
  const poll = now => {
    if (now - lastPoll >= 100) {
      lastPoll = now;
      const pad = navigator.getGamepads && navigator.getGamepads()[0];
      if (pad) {
        const snapshot = JSON.stringify({ axes: pad.axes.map(quantize), buttons: pad.buttons.map(b => +b.value.toFixed(2)) });
        if (snapshot !== previous) { previous = snapshot; write('gamepad', snapshot); }
      }
    }
    requestAnimationFrame(poll);
  };
  write('ready', 'inspect this page and PrismaControllerTest_F4.log while pressing the controller');
  requestAnimationFrame(poll);
})();
