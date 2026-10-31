'use strict';

const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const VIEWS = path.resolve(__dirname, '..', 'assets', 'views');
const INPUT = fs.readFileSync(path.join(VIEWS, 'prisma_input.js'), 'utf8');
const OSK = fs.readFileSync(path.join(VIEWS, 'prisma_osk.js'), 'utf8');

function load() {
  const listeners = {};
  function Event(type, init) {
    this.type = type;
    this.bubbles = !!(init && init.bubbles);
  }
  const window = {
    Event,
    addEventListener(name, fn) {
      (listeners[name] = listeners[name] || []).push(fn);
    },
    dispatch(name, detail) {
      const event = {
        detail,
        immediateStopped: false,
        stopImmediatePropagation() {
          this.immediateStopped = true;
        },
      };
      for (const fn of listeners[name] || []) {
        fn(event);
        if (event.immediateStopped) break;
      }
      return event;
    },
  };
  const document = { activeElement: null, defaultView: window };
  window.document = document;
  const context = vm.createContext({ window, document, Event, console });
  vm.runInContext(INPUT, context);
  vm.runInContext(OSK, context);
  window.element = (tagName, type) => {
    const own = {};
    return {
      tagName,
      type: type || '',
      value: '',
      maxLength: -1,
      readOnly: false,
      disabled: false,
      textContent: '',
      isContentEditable: false,
      isConnected: true,
      ownerDocument: document,
      hidden: false,
      innerHTML: '',
      addEventListener(name, fn) {
        (own[name] = own[name] || []).push(fn);
      },
      dispatchEvent(event) {
        event.target = this;
        (own[event.type] || []).forEach((fn) => fn(event));
        return true;
      },
    };
  };
  window.editable = (text) => {
    const el = window.element('DIV');
    delete el.value;
    el.isContentEditable = true;
    el.textContent = text;
    return el;
  };
  window.appActions = [];
  window.press = (button, state) =>
    window.dispatch('prisma-controller-action', {
      action: 'app',
      button,
      state: state || 'pressed',
    });
  return window;
}

function withApp(window) {
  window.addEventListener('prisma-controller-action', (event) =>
    window.appActions.push(event.detail)
  );
  return window;
}

module.exports = { load, withApp };
