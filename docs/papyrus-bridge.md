---
title: 'Papyrus Bridge'
---
# Papyrus Bridge

PrismaUI_F4 2.1.0 injects `window.prisma` into each Prisma view immediately before the framework dispatches that view's `OnDomReadyCallback`. The bridge lets page JavaScript read selected Fallout/Papyrus values, write values owned by the view's plugin, and emit a named PrismaUI Papyrus event.

The bridge is **not unrestricted cross-mod access**. PrismaUI associates every view with the F4SE plugin that created it and enforces an ESP ownership policy on bridge requests.

## Availability and timing

By the time your C++ `OnDomReadyCallback` runs, `window.prisma` is installed for that document.

```cpp
static void OnDomReady(PrismaView view)
{
    g_api->Invoke(view, "window.init && window.init()");
}
```

Do not assume `window.prisma` exists in page scripts that execute before DOM-ready.

## Access policy

The view owner is the F4SE plugin that created the view.

For global/property access:

- a view may read and write the ESP/ESL whose filename stem matches its owning F4SE plugin;
- vanilla masters (`Fallout4.esm` and the official DLC masters) may be **read** from any view;
- vanilla masters may not be written by a foreign view;
- unrelated third-party plugin ESPs are rejected.

Example: a view owned by `MyPlugin_F4.dll` may access `MyPlugin_F4.esp`/`.esl`. It may read `Fallout4.esm`, but it cannot use this bridge to write another mod's ESP state.

A denied **read** request rejects its Promise. A missing/invalid form or property for an otherwise allowed request resolves to `null`.

## Form ID format

`formId` is the plugin-local hexadecimal FormID without the load-order prefix.

| Local value | Pass as |
|---|---|
| `00000800` | `"800"` |
| `00000D63` | `"D63"` |

PrismaUI resolves normal and light-plugin compile indexes internally.

## `prisma.getGlobal(esp, formId)`

Reads an allowed `TESGlobal` value.

```js
try {
  const value = await window.prisma.getGlobal('MyPlugin_F4.esp', '800');
  if (value === null) {
    // Allowed request, but the plugin/form/global was not available.
  }
} catch (err) {
  // Access-policy or malformed-request failure.
}
```

**Returns:** `Promise<number | null>`.

## `prisma.setGlobal(esp, formId, value)`

Queues a game-thread write to an allowed `TESGlobal`.

```js
window.prisma.setGlobal('MyPlugin_F4.esp', '800', 2.5);
```

This is fire-and-forget. There is no JavaScript success result. Read the value back when confirmation matters.

## `prisma.getProperty(esp, formId, scriptName, propName)`

Reads an attached Papyrus property whose current value is one of the bridge's supported scalar types.

```js
const value = await window.prisma.getProperty(
  'MyPlugin_F4.esp',
  '800',
  'MyPlugin_QuestScript',
  'Difficulty'
);
```

Supported returned values:

- `number` for Papyrus `float` and `int` properties;
- `boolean` for Papyrus `bool` properties;
- `null` when the allowed form/script/property/value cannot be resolved.

Denied access rejects the Promise.

## `prisma.setProperty(esp, formId, scriptName, propName, value)`

Writes an attached Papyrus `float`, `int`, or `bool` property owned by the view's plugin.

```js
window.prisma.setProperty(
  'MyPlugin_F4.esp',
  '800',
  'MyPlugin_QuestScript',
  'DamageScale',
  1.5
);

window.prisma.setProperty(
  'MyPlugin_F4.esp',
  '800',
  'MyPlugin_QuestScript',
  'HardcoreMode',
  true
);
```

The 2.1.0 request parser supports numeric and boolean scalar values for this write path. **Do not pass strings expecting automatic conversion.** Strings and arrays are not a supported property-write contract.

Writes are fire-and-forget and update the backing attached property value directly. They do not call custom Papyrus getter/setter functions or synthesize your script's own events.

## `prisma.emit(eventName, data)`

Sends an external Papyrus event through PrismaUI's registered event channel.

```js
window.prisma.emit('settingsSaved', { profile: 'survival' });
```

Non-string data is JSON-stringified before dispatch. Event names must be non-empty and no longer than 128 characters.

PrismaUI dispatches the fixed external event `PrismaUI_Event` with two string arguments:

1. the event name supplied by JavaScript;
2. the string/JSON payload.

This operation is fire-and-forget.

## Threading

Bridge requests that touch Fallout or Papyrus state are queued through the F4SE task interface and execute on the game thread.

Read operations resolve their JavaScript Promises later by invoking a resolver back in the originating view. If the view is destroyed before the queued work completes, a result cannot be delivered.

## VM readiness

Property access requires the target form's attached script and the Papyrus VM to be available. Perform property work from normal loaded-game/new-game UI rather than during early plugin startup.

A property read that cannot find the VM, attached script, property, or supported value resolves to `null`.

## Separate Papyrus-native surface

PrismaUI also registers native Papyrus functions under the `PrismaUI` script for named view lifecycle, focus, offscreen sizing, and mesh binding. That is a separate **Papyrus -> PrismaUI** API from the `window.prisma` **JavaScript -> game/Papyrus** bridge documented here.

See the shipped `scripts/Source/PrismaUI.psc` for the exact native Papyrus surface.

## Security guidance

Treat JavaScript payloads as application input even though PrismaUI validates bridge requests. Do not load arbitrary external HTML and then treat the bridge as an application security sandbox.
