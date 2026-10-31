(function () {
  "use strict";

  var DEFAULT_LAYOUTS = {
    lower: ["1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"],
    upper: ["1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"],
    symbol: ["1234567890", ":-_./\\@", "+*=?!&%$", "#()[]{}<>"]
  };

  var ACTIONS = [
    { id: "back", label: "Del" },
    { id: "space", label: "Space" },
    { id: "paste", label: "Paste" },
    { id: "done", label: "Done" }
  ];

  var MOVES = { DUp: [-1, 0], DDown: [1, 0], DLeft: [0, -1], DRight: [0, 1] };
  var BUTTONS = { A: 1, B: 1, LB: 1, RB: 1, DUp: 1, DDown: 1, DLeft: 1, DRight: 1 };

  var state = {
    open: false,
    target: null,
    original: "",
    layouts: DEFAULT_LAYOUTS,
    names: Object.keys(DEFAULT_LAYOUTS),
    layout: 0,
    row: 0,
    col: 0,
    maxLength: Infinity,
    host: null,
    session: 0
  };
  var held = {};

  function rows() {
    return (state.layouts[state.names[state.layout]] || []).map(function (line) {
      return typeof line === "string" ? line.split("") : line.slice();
    });
  }

  function isField(node) {
    var tag = String(node.tagName || "").toUpperCase();
    return tag === "INPUT" || tag === "TEXTAREA";
  }

  function readValue(node) {
    var value = isField(node) ? node.value : node.textContent;
    return value === null || value === undefined ? "" : String(value);
  }

  function isLive(node) {
    if (!node || node.isConnected === false) return false;
    var root = node.ownerDocument && node.ownerDocument.documentElement;
    return !root || typeof root.contains !== "function" || root.contains(node);
  }

  function writeValue(value) {
    var target = state.target;
    if (isField(target)) target.value = value;
    else target.textContent = value;
    var view = target.ownerDocument && target.ownerDocument.defaultView;
    var EventCtor = (view && view.Event) || window.Event;
    target.dispatchEvent(new EventCtor("input", { bubbles: true }));
  }

  function append(text) {
    var value = readValue(state.target);
    if (value.length + text.length <= state.maxLength) writeValue(value + text);
  }

  function paste(text) {
    if (!state.open || !isLive(state.target)) return false;
    text = String(text === null || text === undefined ? "" : text).replace(/\r\n?/g, "\n");
    if (String(state.target.tagName || "").toUpperCase() === "INPUT") text = text.replace(/\n/g, " ");
    var value = readValue(state.target);
    var room = state.maxLength - value.length;
    if (text.length > room) text = text.slice(0, Math.max(0, room)).replace(/[\uD800-\uDBFF]$/, "");
    if (!text) return false;
    writeValue(value + text);
    return true;
  }

  function requestPaste() {
    var bridge = window.prisma;
    if (!bridge || typeof bridge.readClipboard !== "function") return;
    var session = state.session;
    Promise.resolve(bridge.readClipboard()).then(function (text) {
      if (state.session === session) paste(text);
    }, function () {});
  }

  function finish() {
    state.open = false;
    state.target = null;
    state.original = "";
    render();
  }

  function close(commit) {
    if (!state.open) return false;
    if (!commit && isLive(state.target)) writeValue(state.original);
    finish();
    return true;
  }

  function clamp() {
    var lines = rows();
    state.row = Math.max(0, Math.min(state.row, lines.length));
    var length = state.row < lines.length ? lines[state.row].length : ACTIONS.length;
    state.col = Math.max(0, Math.min(state.col, length - 1));
  }

  function activate() {
    var lines = rows();
    if (state.row < lines.length) return append(lines[state.row][state.col]);
    var id = ACTIONS[state.col].id;
    if (id === "space") append(" ");
    else if (id === "paste") requestPaste();
    else if (id === "back") writeValue(Array.from(readValue(state.target)).slice(0, -1).join(""));
    else close(true);
  }

  function esc(value) {
    return String(value)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;");
  }

  function key(label, focused, extra) {
    return '<span class="prisma-osk-key' + (extra || "") + (focused ? " focused" : "") + '">' + esc(label) + "</span>";
  }

  function render() {
    var host = state.host;
    if (!host) return;
    host.hidden = !state.open;
    if (!state.open) {
      host.innerHTML = "";
      return;
    }
    var lines = rows();
    var html = lines.map(function (line, r) {
      return '<div class="prisma-osk-row">' + line.map(function (label, c) {
        return key(label, r === state.row && c === state.col);
      }).join("") + "</div>";
    });
    html.push('<div class="prisma-osk-row prisma-osk-actions">' + ACTIONS.map(function (action, c) {
      return key(action.label, state.row === lines.length && c === state.col, " prisma-osk-key-wide");
    }).join("") + "</div>");
    host.innerHTML = html.join("");
  }

  function handleButton(button) {
    if (!state.open || !BUTTONS[button]) return false;
    if (!isLive(state.target)) {
      finish();
      return true;
    }
    if (button === "B") return close(false);
    if (button === "A") activate();
    else if (button === "LB" || button === "RB") {
      var count = state.names.length;
      state.layout = (state.layout + (button === "RB" ? 1 : -1) + count) % count;
      clamp();
    } else {
      state.row += MOVES[button][0];
      state.col += MOVES[button][1];
      clamp();
    }
    render();
    return true;
  }

  function onControllerAction(event) {
    var detail = event && event.detail;
    if (!detail) return;
    var button = String(detail.button || "");
    var consumed = false;
    if (detail.state === "pressed") {
      consumed = handleButton(button);
      if (consumed) held[button] = true;
      else delete held[button];
    } else if (detail.state === "repeat") {
      consumed = !!held[button];
      if (consumed && state.open && MOVES[button]) handleButton(button);
    } else if (detail.state === "released") {
      consumed = !!held[button];
      delete held[button];
    }
    if (consumed) event.stopImmediatePropagation();
  }

  window.PrismaOSK = {
    bindHost: function (host) {
      state.host = host || null;
      render();
    },

    open: function (element, options) {
      options = options || {};
      if (!window.PrismaInput.isTextEntryTarget(element) || element.readOnly || element.disabled) return false;
      var layouts = options.layouts && typeof options.layouts === "object" ? options.layouts : DEFAULT_LAYOUTS;
      var names = Object.keys(layouts);
      if (!names.length) return false;
      var limit = Number.isFinite(options.maxLength) ? options.maxLength : element.maxLength;
      state.layouts = layouts;
      state.names = names;
      state.layout = 0;
      state.row = 0;
      state.col = 0;
      state.maxLength = Number.isFinite(limit) && limit >= 0 ? Math.floor(limit) : Infinity;
      state.target = element;
      state.original = readValue(element);
      state.session += 1;
      state.open = true;
      render();
      return true;
    },

    paste: paste,

    close: function (options) {
      return close(!options || options.commit !== false);
    },

    isOpen: function () {
      return state.open;
    },

    getTarget: function () {
      return state.target;
    },

    owns: function (element) {
      return state.open && !!element && state.target === element;
    },

    getState: function () {
      return {
        open: state.open,
        layout: state.open ? state.names[state.layout] : null,
        row: state.row,
        col: state.col,
        value: state.target ? readValue(state.target) : ""
      };
    },

    handleButton: handleButton,
    render: render
  };

  window.addEventListener("prisma-controller-action", onControllerAction);
})();
