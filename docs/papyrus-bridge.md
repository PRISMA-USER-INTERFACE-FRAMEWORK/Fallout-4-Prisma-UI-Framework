---
title: 'Papyrus bridge'
---

# Papyrus bridge

There are two Papyrus-related things in PrismaUI, and they point in opposite directions:

- **`window.prisma`** — JavaScript inside a view reads and writes selected game values and emits a Papyrus event. This page.
- **`PrismaUI.psc`** — Papyrus creates and controls a view (show, hide, focus, bind to geometry). For that, see [Open a panel from Papyrus](cookbook/from-papyrus).

The JavaScript bridge is not a general "call any Papyrus function" tunnel. It is a small, ownership-checked surface.

## What you can do from the page

```js
await window.prisma.getGlobal('MyPlugin_F4.esp', '800');    // read a global
window.prisma.setGlobal('MyPlugin_F4.esp', '800', 2.5);     // write a global
await window.prisma.getProperty('MyPlugin_F4.esp', '800', 'MyPlugin_QuestScript', 'Difficulty');
window.prisma.setProperty('MyPlugin_F4.esp', '800', 'MyPlugin_QuestScript', 'DamageScale', 1.5);
window.prisma.emit('settingsSaved', { profile: 'survival' }); // Papyrus event
```

## Who owns what

Each view belongs to the F4SE plugin that created it. That ownership decides what the page may touch:

- **Your own ESP/ESL** (file name stem matching your plugin) — read and write.
- **`Fallout4.esm` and the official DLC masters** — read from any view.
- **Other mods' ESPs** — rejected.
- **Writing a vanilla master** — rejected.

Denied requests reject the Promise. Missing forms, scripts or properties for an *allowed* request resolve to `null` instead.

## Form IDs

Pass the plugin-local hexadecimal form ID, without the load-order prefix:

| You know it as | Pass |
| --- | --- |
| `00000800` | `"800"` |
| `00000D63` | `"D63"` |

PrismaUI resolves the compile index for normal and light plugins for you.

## Globals

Read (returns `Promise<number | null>`):

```js
try {
  const value = await window.prisma.getGlobal('MyPlugin_F4.esp', '800');
  if (value === null) {
    // Allowed request, but the form/global was not available yet.
  }
} catch (err) {
  // Access denied or malformed request.
}
```

Write (fire-and-forget):

```js
window.prisma.setGlobal('MyPlugin_F4.esp', '800', 2.5);
```

A write has no JavaScript success result. Read the value back if you need confirmation.

## Properties

Reads return `number`, `boolean`, or `null`:

```js
const difficulty = await window.prisma.getProperty(
  'MyPlugin_F4.esp',
  '800',
  'MyPlugin_QuestScript',
  'Difficulty'
);
```

Writes accept numbers and booleans only:

```js
window.prisma.setProperty('MyPlugin_F4.esp', '800', 'MyPlugin_QuestScript', 'HardcoreMode', true);
```

- No strings, arrays or automatic conversion. A string value fails the write.
- The write goes to the attached property value directly. It does **not** call your script's getter/setter functions and does not fire your script's events.
- Writes are fire-and-forget.

## Emitting a Papyrus event

```js
window.prisma.emit('settingsSaved', { profile: 'survival' });
```

- Non-string payloads are JSON-stringified before dispatch.
- The event name must be non-empty and at most 128 characters.
- Papyrus receives the fixed external event `PrismaUI_Event(name, payload)`.

That event is fire-and-forget too. On the Papyrus side, listen for it:

```papyrus
Event OnExternalEvent(string eventName, string payload)
    if eventName == "settingsSaved"
        ; payload is a JSON string
    endif
EndEvent
```

## Threading and lifetime

Bridge work that touches game state runs through the F4SE task interface on the game thread.

- JavaScript read Promises resolve later, by calling back into the originating view.
- If the view is destroyed before the work finishes, the result has nowhere to go. Guard for a dead view on the page side.
- Property access needs the Papyrus VM and the target's attached script to exist. Use the bridge from normal loaded-game UI, not from early startup.

## When a read returns null

In order of likelihood:

1. the form, script or property does not exist yet;
2. the value is not a supported scalar type;
3. the request happened before the VM/script was ready.

`null` means "allowed request, nothing usable" — it is not an access denial. Denials throw.

## Security

PrismaUI validates every request, but treat JavaScript input as application input. Do not load arbitrary external HTML and then rely on the bridge as a sandbox — the sandbox assumes the page is yours.

## Where to go next

- [Open a panel from Papyrus](cookbook/from-papyrus) — the other direction
- [The HTML side](html-views) — page basics
- [Translations](translations) — the usual alternative to globals for UI text
- [Limitations](limitations) — what is not exposed
