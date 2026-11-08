(function (root) {
  'use strict';

  var STAGE_W = 1920;
  var STAGE_H = 1080;
  var MIN_SIZE = 32;
  var SNAP_STEPS = [0, 5, 10, 20];
  var COALESCE_MS = 400;

  function clamp(value, low, high) {
    return Math.min(Math.max(value, low), high);
  }

  function snapValue(value, step) {
    return step > 0 ? Math.round(value / step) * step : value;
  }

  function clampRect(rect) {
    var w = clamp(rect.w, MIN_SIZE, STAGE_W * 2);
    var h = clamp(rect.h, MIN_SIZE, STAGE_H * 2);
    return {
      x: clamp(rect.x, MIN_SIZE - w, STAGE_W - MIN_SIZE),
      y: clamp(rect.y, MIN_SIZE - h, STAGE_H - MIN_SIZE),
      w: w,
      h: h,
    };
  }

  function rectsEqual(a, b) {
    return a.x === b.x && a.y === b.y && a.w === b.w && a.h === b.h;
  }

  function moveRect(rect, dx, dy, step) {
    return clampRect({ x: snapValue(rect.x + dx, step), y: snapValue(rect.y + dy, step), w: rect.w, h: rect.h });
  }

  function resizeRect(rect, handle, dx, dy, step) {
    var left = rect.x;
    var top = rect.y;
    var right = rect.x + rect.w;
    var bottom = rect.y + rect.h;
    if (handle.indexOf('w') !== -1) left = Math.min(snapValue(left + dx, step), right - MIN_SIZE);
    if (handle.indexOf('e') !== -1) right = Math.max(snapValue(right + dx, step), left + MIN_SIZE);
    if (handle.indexOf('n') !== -1) top = Math.min(snapValue(top + dy, step), bottom - MIN_SIZE);
    if (handle.indexOf('s') !== -1) bottom = Math.max(snapValue(bottom + dy, step), top + MIN_SIZE);
    return clampRect({ x: left, y: top, w: right - left, h: bottom - top });
  }

  function cycleSnap(current) {
    var index = SNAP_STEPS.indexOf(current);
    return SNAP_STEPS[(index + 1) % SNAP_STEPS.length];
  }

  function neighbour(ids, current, direction) {
    if (!ids.length) return null;
    var index = ids.indexOf(current);
    if (index === -1) return direction < 0 ? ids[ids.length - 1] : ids[0];
    return ids[(index + direction + ids.length) % ids.length];
  }

  function createHistory(limit) {
    var undoStack = [];
    var redoStack = [];
    var max = limit || 100;
    return {
      push: function (entry, now) {
        var last = undoStack[undoStack.length - 1];
        var stamp = typeof now === 'number' ? now : 0;
        redoStack.length = 0;
        if (
          last &&
          entry.key &&
          last.key === entry.key &&
          last.id === entry.id &&
          stamp - last.stamp <= COALESCE_MS
        ) {
          last.after = entry.after;
          last.stamp = stamp;
          return;
        }
        undoStack.push({ id: entry.id, key: entry.key || null, before: entry.before, after: entry.after, stamp: stamp });
        if (undoStack.length > max) undoStack.shift();
      },
      undo: function () {
        var entry = undoStack.pop();
        if (!entry) return null;
        redoStack.push(entry);
        return { id: entry.id, state: entry.before };
      },
      redo: function () {
        var entry = redoStack.pop();
        if (!entry) return null;
        undoStack.push(entry);
        return { id: entry.id, state: entry.after };
      },
      canUndo: function () {
        return undoStack.length > 0;
      },
      canRedo: function () {
        return redoStack.length > 0;
      },
      clear: function () {
        undoStack.length = 0;
        redoStack.length = 0;
      },
    };
  }

  function parseLayout(payload) {
    var data = payload;
    if (typeof payload === 'string') {
      try {
        data = JSON.parse(payload);
      } catch (e) {
        return null;
      }
    }
    if (!data || typeof data !== 'object' || !Array.isArray(data.views)) return null;
    var views = [];
    data.views.forEach(function (view) {
      if (!view || typeof view.id !== 'number' || !Array.isArray(view.rect) || view.rect.length !== 4) return;
      var rect = { x: +view.rect[0], y: +view.rect[1], w: +view.rect[2], h: +view.rect[3] };
      var fallback = Array.isArray(view.default) && view.default.length === 4 ? view.default : [0, 0, STAGE_W, STAGE_H];
      if (![rect.x, rect.y, rect.w, rect.h].every(isFinite)) return;
      views.push({
        id: view.id,
        title: typeof view.title === 'string' && view.title ? view.title : 'View ' + view.id,
        owner: typeof view.owner === 'string' ? view.owner : '',
        registered: !!view.registered,
        hidden: !!view.hidden,
        rect: rect,
        defaultRect: { x: +fallback[0], y: +fallback[1], w: +fallback[2], h: +fallback[3] },
      });
    });
    return {
      snap: SNAP_STEPS.indexOf(data.snap) === -1 ? 10 : data.snap,
      grid: !!data.grid,
      views: views,
    };
  }

  function message(channel, payload) {
    return channel + '\n' + payload;
  }

  function rectMessage(id, rect) {
    return message('rect', [id, rect.x, rect.y, rect.w, rect.h].join(','));
  }

  root.PrismaLayoutCore = {
    STAGE_W: STAGE_W,
    STAGE_H: STAGE_H,
    MIN_SIZE: MIN_SIZE,
    SNAP_STEPS: SNAP_STEPS,
    snapValue: snapValue,
    clampRect: clampRect,
    rectsEqual: rectsEqual,
    moveRect: moveRect,
    resizeRect: resizeRect,
    cycleSnap: cycleSnap,
    neighbour: neighbour,
    createHistory: createHistory,
    parseLayout: parseLayout,
    message: message,
    rectMessage: rectMessage,
  };
})(typeof globalThis !== 'undefined' ? globalThis : this);
