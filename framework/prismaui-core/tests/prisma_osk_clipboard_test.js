"use strict";

const assert = require("node:assert/strict");
const test = require("node:test");
const { load } = require("./prisma_osk_test_support.js");

const flush = () => new Promise((resolve) => setImmediate(resolve));

function clipboard(w, text) {
  const calls = [];
  w.prisma = {
    readClipboard() {
      calls.push(1);
      return Promise.resolve(text);
    }
  };
  return calls;
}

function focusPaste(w) {
  for (let i = 0; i < 9; i++) w.PrismaOSK.handleButton("DDown");
  w.PrismaOSK.handleButton("DRight");
  w.PrismaOSK.handleButton("DRight");
}

test("Paste key reads the clipboard bridge and appends with one input event", async () => {
  const w = load();
  const calls = clipboard(w, "Sanctuary");
  const input = w.element("INPUT", "text");
  input.value = "New ";
  const seen = [];
  input.addEventListener("input", (event) => seen.push(event));
  w.PrismaOSK.bindHost(w.element("DIV"));
  w.PrismaOSK.open(input);
  focusPaste(w);
  assert.equal(w.press("A").immediateStopped, true);
  await flush();
  assert.equal(calls.length, 1);
  assert.equal(input.value, "New Sanctuary");
  assert.equal(seen.length, 1);
  assert.equal(seen[0].target, input);
  assert.equal(seen[0].bubbles, true);
  assert.equal(w.PrismaOSK.isOpen(), true);
});

test("paste flattens line breaks for INPUT and keeps them for TEXTAREA", () => {
  const w = load();
  const input = w.element("INPUT", "text");
  w.PrismaOSK.open(input);
  assert.equal(w.PrismaOSK.paste("a\r\nb\rc\nd"), true);
  assert.equal(input.value, "a b c d");
  w.PrismaOSK.close();

  const area = w.element("TEXTAREA");
  w.PrismaOSK.open(area);
  w.PrismaOSK.paste("a\r\nb\rc");
  assert.equal(area.value, "a\nb\nc");
  w.PrismaOSK.close();

  const div = w.editable("x");
  w.PrismaOSK.open(div);
  w.PrismaOSK.paste("y\nz");
  assert.equal(div.textContent, "xy\nz");
});

test("paste truncates to maxLength without splitting a surrogate pair", () => {
  const w = load();
  const input = w.element("INPUT", "text");
  input.value = "ab";
  w.PrismaOSK.open(input, { maxLength: 5 });
  assert.equal(w.PrismaOSK.paste("cd\u{1F600}"), true);
  assert.equal(input.value, "abcd");
  assert.equal(w.PrismaOSK.paste("e"), true);
  assert.equal(w.PrismaOSK.paste("f"), false);
  assert.equal(input.value, "abcde");
});

test("empty, null or unavailable clipboard changes nothing", async () => {
  const w = load();
  const input = w.element("INPUT", "text");
  let events = 0;
  input.addEventListener("input", () => events++);
  w.PrismaOSK.open(input);
  assert.equal(w.PrismaOSK.paste(""), false);
  assert.equal(w.PrismaOSK.paste(null), false);
  focusPaste(w);
  assert.equal(w.PrismaOSK.handleButton("A"), true);
  clipboard(w, null);
  w.PrismaOSK.handleButton("A");
  w.prisma = { readClipboard: () => Promise.reject(new Error("denied")) };
  w.PrismaOSK.handleButton("A");
  await flush();
  assert.equal(input.value, "");
  assert.equal(events, 0);
});

test("a clipboard result arriving after close or reopen is dropped", async () => {
  const w = load();
  clipboard(w, "late");
  const input = w.element("INPUT", "text");
  w.PrismaOSK.open(input);
  focusPaste(w);
  w.PrismaOSK.handleButton("A");
  w.PrismaOSK.handleButton("B");
  await flush();
  assert.equal(input.value, "");

  w.PrismaOSK.open(input);
  focusPaste(w);
  w.PrismaOSK.handleButton("A");
  w.PrismaOSK.close();
  w.PrismaOSK.open(input);
  await flush();
  assert.equal(input.value, "");
});

test("a clipboard result for a detached target does not write stale DOM", async () => {
  const w = load();
  clipboard(w, "late");
  const input = w.element("INPUT", "text");
  w.PrismaOSK.open(input);
  focusPaste(w);
  w.PrismaOSK.handleButton("A");
  input.isConnected = false;
  await flush();
  assert.equal(input.value, "");
  assert.equal(w.PrismaOSK.paste("x"), false);
});

test("action row renders Paste between Space and Done", () => {
  const w = load();
  const host = w.element("DIV");
  w.PrismaOSK.bindHost(host);
  w.PrismaOSK.open(w.element("INPUT", "text"));
  assert.match(host.innerHTML, />Del<.*>Space<.*>Paste<.*>Done</);
});
