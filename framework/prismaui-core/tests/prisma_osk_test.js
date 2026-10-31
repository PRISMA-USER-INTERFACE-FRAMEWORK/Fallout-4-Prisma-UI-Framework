"use strict";

const assert = require("node:assert/strict");
const test = require("node:test");
const { load, withApp } = require("./prisma_osk_test_support.js");

test("PrismaInput recognizes editable form targets", () => {
  const w = load();
  const I = w.PrismaInput;
  assert.equal(I.isTextEntryTarget(w.element("INPUT", "text")), true);
  assert.equal(I.isTextEntryTarget(w.element("INPUT", "password")), true);
  assert.equal(I.isTextEntryTarget(w.element("INPUT", "")), true);
  assert.equal(I.isTextEntryTarget(w.element("TEXTAREA")), true);
  assert.equal(I.isTextEntryTarget(w.editable("")), true);
  assert.equal(I.isTextEntryTarget(w.element("INPUT", "checkbox")), false);
  assert.equal(I.isTextEntryTarget(w.element("DIV")), false);
  assert.equal(I.isTextEntryTarget(null), false);
  assert.equal(I.isFormEntryTarget(w.element("SELECT")), true);
  assert.equal(I.isTextEntryTarget(w.element("SELECT")), false);
});

test("PrismaInput falls back to the focused element", () => {
  const w = load();
  w.document.activeElement = w.element("TEXTAREA");
  assert.equal(w.PrismaInput.isKeyboardCaptureTarget(), true);
  w.document.activeElement = w.element("BUTTON");
  assert.equal(w.PrismaInput.isKeyboardCaptureTarget(), false);
});

test("open refuses non-text, read-only and disabled targets", () => {
  const w = load();
  const readOnly = w.element("INPUT", "text");
  readOnly.readOnly = true;
  const disabled = w.element("TEXTAREA");
  disabled.disabled = true;
  assert.equal(w.PrismaOSK.open(w.element("INPUT", "checkbox")), false);
  assert.equal(w.PrismaOSK.open(readOnly), false);
  assert.equal(w.PrismaOSK.open(disabled), false);
  assert.equal(w.PrismaOSK.open(null), false);
  assert.equal(w.PrismaOSK.isOpen(), false);
});

test("A types into the live target and raises bubbling input from it", () => {
  const w = load();
  const input = w.element("INPUT", "text");
  const seen = [];
  input.addEventListener("input", (event) => seen.push(event));

  assert.equal(w.PrismaOSK.open(input), true);
  assert.equal(w.PrismaOSK.handleButton("A"), true);

  assert.equal(input.value, "1");
  assert.equal(seen.length, 1);
  assert.equal(seen[0].target, input);
  assert.equal(seen[0].type, "input");
  assert.equal(seen[0].bubbles, true);
});

test("textarea and contenteditable targets are edited", () => {
  const w = load();
  const area = w.element("TEXTAREA");
  area.value = "x";
  w.PrismaOSK.open(area);
  w.PrismaOSK.handleButton("A");
  assert.equal(area.value, "x1");
  w.PrismaOSK.close();

  const div = w.editable("a");
  let events = 0;
  div.addEventListener("input", () => events++);
  w.PrismaOSK.open(div);
  w.PrismaOSK.handleButton("A");
  assert.equal(div.textContent, "a1");
  assert.equal(events, 1);
});

test("D-pad navigates, clamps and selects the focused key", () => {
  const w = load();
  const input = w.element("INPUT", "text");
  w.PrismaOSK.open(input);
  w.PrismaOSK.handleButton("DLeft");
  w.PrismaOSK.handleButton("DUp");
  assert.deepEqual([w.PrismaOSK.getState().row, w.PrismaOSK.getState().col], [0, 0]);
  w.PrismaOSK.handleButton("DDown");
  w.PrismaOSK.handleButton("DRight");
  w.PrismaOSK.handleButton("A");
  assert.equal(input.value, "w");
});

test("LB/RB cycle layouts in both directions", () => {
  const w = load();
  const input = w.element("INPUT", "text");
  w.PrismaOSK.open(input);
  w.PrismaOSK.handleButton("DDown");
  w.PrismaOSK.handleButton("RB");
  assert.equal(w.PrismaOSK.getState().layout, "upper");
  w.PrismaOSK.handleButton("A");
  w.PrismaOSK.handleButton("LB");
  w.PrismaOSK.handleButton("LB");
  assert.equal(w.PrismaOSK.getState().layout, "symbol");
  w.PrismaOSK.handleButton("RB");
  w.PrismaOSK.handleButton("A");
  assert.equal(input.value, "Qq");
});

test("action row deletes, spaces, skips Paste and Done commits", () => {
  const w = load();
  const input = w.element("INPUT", "text");
  input.value = "ab";
  w.PrismaOSK.open(input);
  for (let i = 0; i < 9; i++) w.PrismaOSK.handleButton("DDown");
  w.PrismaOSK.handleButton("A");
  assert.equal(input.value, "a");
  w.PrismaOSK.handleButton("DRight");
  w.PrismaOSK.handleButton("A");
  assert.equal(input.value, "a ");
  w.PrismaOSK.handleButton("DRight");
  w.PrismaOSK.handleButton("DRight");
  w.PrismaOSK.handleButton("A");
  assert.equal(w.PrismaOSK.isOpen(), false);
  assert.equal(input.value, "a ");
});

test("Del removes a pasted supplementary character without leaving a surrogate", () => {
  const w = withApp(load());
  const input = w.element("INPUT", "text");
  let events = 0;
  input.addEventListener("input", () => events++);
  w.PrismaOSK.open(input);
  w.PrismaOSK.paste("a\u{1F600}");
  for (let i = 0; i < 4; i++) {
    w.press("DDown");
    w.press("DDown", "released");
  }
  for (const expected of ["a", ""]) {
    assert.equal(w.press("A").immediateStopped, true);
    assert.equal(w.press("A", "released").immediateStopped, true);
    assert.equal(input.value, expected);
  }
  assert.equal(events, 3);
  assert.deepEqual(w.appActions, []);
});

test("B cancels and restores the original value with an input event", () => {
  const w = load();
  const input = w.element("INPUT", "text");
  input.value = "abc";
  let events = 0;
  input.addEventListener("input", () => events++);
  w.PrismaOSK.open(input);
  w.PrismaOSK.handleButton("A");
  w.PrismaOSK.handleButton("B");
  assert.equal(input.value, "abc");
  assert.equal(events, 2);
  assert.equal(w.PrismaOSK.isOpen(), false);

  const div = w.editable("xyz");
  w.PrismaOSK.open(div);
  w.PrismaOSK.handleButton("A");
  w.PrismaOSK.handleButton("B");
  assert.equal(div.textContent, "xyz");
});

test("cancel restores numeric zero exactly", () => {
  const w = load();
  const input = w.element("INPUT", "number");
  input.value = 0;
  w.PrismaOSK.open(input);
  w.PrismaOSK.handleButton("A");
  w.PrismaOSK.handleButton("B");
  assert.equal(input.value, "0");
});

test("maxLength option and element maxLength block further characters", () => {
  const w = load();
  const input = w.element("INPUT", "text");
  input.value = "x";
  w.PrismaOSK.open(input, { maxLength: 1 });
  w.PrismaOSK.handleButton("A");
  assert.equal(input.value, "x");
  w.PrismaOSK.close();

  const limited = w.element("INPUT", "text");
  limited.maxLength = 2;
  w.PrismaOSK.open(limited);
  for (let i = 0; i < 4; i++) w.PrismaOSK.handleButton("A");
  assert.equal(limited.value, "11");
});

test("ownership reports the exact live DOM target only while open", () => {
  const w = load();
  const input = w.element("INPUT", "search");
  w.PrismaOSK.open(input);
  assert.equal(w.PrismaOSK.getTarget(), input);
  assert.equal(w.PrismaOSK.owns(input), true);
  assert.equal(w.PrismaOSK.owns(w.element("INPUT", "search")), false);
  w.PrismaOSK.close();
  assert.equal(w.PrismaOSK.owns(input), false);
  assert.equal(w.PrismaOSK.getTarget(), null);
});

test("detached target fails closed without mutating stale DOM", () => {
  const w = load();
  const input = w.element("INPUT", "text");
  input.value = "abc";
  w.PrismaOSK.open(input);
  w.PrismaOSK.handleButton("A");
  const edited = input.value;
  input.isConnected = false;

  assert.equal(w.PrismaOSK.handleButton("B"), true);
  assert.equal(input.value, edited);
  assert.equal(w.PrismaOSK.isOpen(), false);
  assert.equal(w.PrismaOSK.getTarget(), null);

  const second = w.element("INPUT", "text");
  w.PrismaOSK.open(second);
  w.PrismaOSK.handleButton("A");
  second.isConnected = false;
  assert.equal(w.PrismaOSK.close({ commit: false }), true);
  assert.equal(second.value, "1");
});

test("open OSK consumes controller actions before application handlers", () => {
  const w = withApp(load());
  const input = w.element("INPUT", "text");
  w.PrismaOSK.open(input);

  const pressed = w.press("A");
  const released = w.press("A", "released");
  const move = w.press("DRight");
  const repeat = w.press("DRight", "repeat");
  const moveReleased = w.press("DRight", "released");

  assert.equal(input.value, "1");
  assert.equal(pressed.immediateStopped, true);
  assert.equal(released.immediateStopped, true);
  assert.equal(move.immediateStopped, true);
  assert.equal(repeat.immediateStopped, true);
  assert.equal(moveReleased.immediateStopped, true);
  assert.equal(w.PrismaOSK.getState().col, 2);
  assert.deepEqual(w.appActions, []);
});

test("detached target consumes the triggering action", () => {
  const w = withApp(load());
  const input = w.element("INPUT", "text");
  w.PrismaOSK.open(input);
  input.isConnected = false;
  const event = w.press("A");
  assert.equal(w.PrismaOSK.isOpen(), false);
  assert.equal(event.immediateStopped, true);
  assert.equal(input.value, "");
  assert.deepEqual(w.appActions, []);
});

test("release of a press consumed by the OSK stays consumed after it closes", () => {
  const w = withApp(load());
  w.PrismaOSK.open(w.element("INPUT", "text"));
  w.press("B");
  assert.equal(w.PrismaOSK.isOpen(), false);
  assert.equal(w.press("B", "released").immediateStopped, true);
  assert.deepEqual(w.appActions, []);
  assert.equal(w.press("B", "released").immediateStopped, false);
});

test("release of a press the application saw is delivered to the application", () => {
  const w = withApp(load());
  const input = w.element("INPUT", "text");
  w.press("A");
  w.PrismaOSK.open(input);
  const event = w.press("A", "released");
  assert.equal(event.immediateStopped, false);
  assert.equal(input.value, "");
  assert.deepEqual(w.appActions.map((d) => d.state), ["pressed", "released"]);
});

test("opening mid-press leaves that press's repeat and release with the application", () => {
  const w = withApp(load());
  assert.equal(w.press("DRight").immediateStopped, false);
  w.PrismaOSK.open(w.element("INPUT", "text"));
  assert.equal(w.press("DRight", "repeat").immediateStopped, false);
  assert.equal(w.PrismaOSK.getState().col, 0);
  assert.equal(w.press("DRight", "released").immediateStopped, false);
  assert.deepEqual(w.appActions.map((d) => d.state), ["pressed", "repeat", "released"]);
});

test("closed OSK does not consume controller actions", () => {
  const w = withApp(load());
  for (const button of ["A", "B", "DUp", "LB", "RB"]) {
    assert.equal(w.press(button).immediateStopped, false);
    assert.equal(w.press(button, "released").immediateStopped, false);
  }
  assert.equal(w.appActions.length, 10);
});

test("unhandled buttons fall through while open", () => {
  const w = withApp(load());
  w.PrismaOSK.open(w.element("INPUT", "text"));
  assert.equal(w.press("Start").immediateStopped, false);
  assert.equal(w.press("X").immediateStopped, false);
  assert.equal(w.appActions.length, 2);
});

test("only canonical Prisma button names are accepted", () => {
  const w = load();
  const input = w.element("INPUT", "text");
  w.PrismaOSK.open(input);
  assert.equal(w.PrismaOSK.handleButton("dright"), false);
  assert.equal(w.PrismaOSK.handleButton("DRight"), true);
  w.press("A");
  assert.equal(input.value, "2");
});

test("bound host renders the focused key and hides when closed", () => {
  const w = load();
  const host = w.element("DIV");
  w.PrismaOSK.bindHost(host);
  assert.equal(host.hidden, true);
  w.PrismaOSK.open(w.element("INPUT", "text"));
  assert.equal(host.hidden, false);
  assert.match(host.innerHTML, /prisma-osk-key focused">1</);
  w.PrismaOSK.close();
  assert.equal(host.hidden, true);
  assert.equal(host.innerHTML, "");
});

test("a lost release does not swallow the application's next press pair", () => {
  const w = withApp(load());
  w.PrismaOSK.open(w.element("INPUT", "text"));
  w.press("B");
  w.press("B");
  assert.equal(w.press("B", "released").immediateStopped, false);
  assert.deepEqual(w.appActions.map((d) => d.state), ["pressed", "released"]);
});
