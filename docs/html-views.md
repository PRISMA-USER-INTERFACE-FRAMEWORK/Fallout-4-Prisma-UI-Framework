---
title: The HTML side
sidebar_label: The HTML side
sidebar_position: 4
---

# The HTML side

This page is about the page half of your mod: where the files live, what the plugin can call, and how the page calls the plugin back.

If you have not built a panel yet, do [Your first panel](quick-start) first. This page assumes you have a view showing on screen.

## Where the files live

Put everything under one folder per plugin:

```text
Data/PrismaUI_F4/views/MyPlugin/
├── index.html
├── app.css
├── app.js
├── fonts/
│   └── ui.woff2
└── images/
    └── icon.webp
```

Create the page with the path relative to the views folder:

```cpp
g_view = g_api->CreateView("MyPlugin/index.html", OnDomReady);
```

Do not pass an absolute path or a `file://` URL. PrismaUI only loads content from your view folder, on purpose.

Link local files normally:

```html
<link rel="stylesheet" href="app.css">
<script src="app.js"></script>
```

and local fonts:

```css
@font-face {
  font-family: 'MyUI';
  src: url('fonts/ui.woff2') format('woff2');
}
```

Anything you ship must be in this folder. Google Fonts, CDNs and remote scripts are not available — and should not be, since they would break offline and on consoles.

## The plugin calls the page

There are two calls. Use `Invoke` for one-off JavaScript, and `InteropCall` for "call this function of mine with this data".

`Invoke` takes a JavaScript string:

```cpp
g_api->Invoke(view, "window.setStatus('Loaded')");
```

The page defines the function being called:

```js
window.setStatus = function (text) {
  document.getElementById('status').textContent = text;
};
```

`InteropCall` calls one named function with a string payload. JSON is the normal payload:

```cpp
g_api->InteropCall(view, "onPlayerData", R"({"hp":210,"ap":75})");
```

```js
window.onPlayerData = function (json) {
  const data = JSON.parse(json);
  document.getElementById('hp').textContent = data.hp;
};
```

Keep the call and the function name identical, including case. If the page does not define it yet, nothing happens — so send data from `OnDomReady` or later, not before.

## The page calls the plugin

Register a listener before the page can call it. This happens in `OnDomReady`:

```cpp
static void OnDomReady(PrismaView view)
{
    g_api->BindUIEvent(view, "requestClose", [](const char*) {
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
    });

    g_api->BindUIEvent(view, "saveSetting", [](const char* json) {
        // json is the string the page passed.
        REX::INFO("page sent: {}", json ? json : "(null)");
    });
}
```

The page calls those names as plain functions:

```js
document.getElementById('close').addEventListener('click', function () {
  window.requestClose();
});

document.getElementById('save').addEventListener('click', function () {
  window.saveSetting(JSON.stringify({ volume: 0.7 }));
});
```

`BindUIEvent` and `RegisterJSListener` both run your callback on the game thread, so you can read and write game state directly inside them. You do not need to wrap them in another task.

A complete two-way example lives in the [Cookbook](cookbook/talk-to-your-plugin).

## Controller events

Controller actions you bind with `BindControllerAction` arrive as a DOM event on the focused page:

```js
window.addEventListener('prisma-controller-action', function (event) {
  const { action, button, state } = event.detail;
  if (state !== 'pressed') return;

  if (action === 'menu.close') window.requestClose();
  if (action === 'menu.next') nextTab();
});
```

There is a full example in [A controller-friendly menu](cookbook/controller-menu).

## Console output

Normal `console.log`, `console.warn` and `console.error` calls work. To see them in your plugin log:

```cpp
g_api->RegisterConsoleCallback(view, [](PrismaView, PRISMA_UI_API::ConsoleMessageLevel level, const char* message) {
    switch (level) {
    case PRISMA_UI_API::ConsoleMessageLevel::Error:   REX::CRITICAL("[JS] {}", message); break;
    case PRISMA_UI_API::ConsoleMessageLevel::Warning: REX::WARN("[JS] {}", message); break;
    default:                                          REX::INFO("[JS] {}", message); break;
    }
});
```

Register this first thing in `OnDomReady`, so errors that happen later in your page actually reach the log.

## The inspector

Press **F12** while a PrismaUI view has focus, when the packaged inspector is available, to open the in-game DevTools. You get Elements, Console, Network and the rest — the same idea as browser DevTools. It is the fastest way to find a broken selector or a JavaScript error.

## What the page can and cannot do

It is a real page for normal web work: DOM, events, timers, Promises, `JSON`, `fetch` to local files inside your view folder, CSS animations, canvas and so on.

It is not a full desktop browser, and some things are turned off on purpose:

- **No WebSockets, EventSource, Web Workers, Shared Workers, WebTransport or WebRTC.** If your mod needs a socket, do it in C++ and send the data to the page with `InteropCall`.
- **No fetch to arbitrary internet hosts.** Remote HTTP belongs in C++.
- **No `file://` access outside your view folder.**
- **No browser storage as a save file.** `localStorage` may survive the session, but the real save data belongs to your plugin or Papyrus.

If something you expect from Chrome is missing, check [Limitations](limitations) before assuming it is a bug.

## Layout and transparency

A view covers the game window, so transparent parts matter:

```css
html, body {
  width: 100%;
  height: 100%;
  margin: 0;
  background: transparent;
}

.panel {
  width: min(700px, 90vw);
  margin: 12vh auto 0;
  background: rgba(8, 10, 9, 0.94);
}
```

Use responsive units and avoid hard-coded 1920×1080 coordinates. The game can run at any resolution and in windowed or borderless mode.

For a floating HUD you normally want a transparent page with a small opaque element. For a full-screen menu you want an opaque backdrop. Both are CSS decisions made in your page; see [A click-through HUD overlay](cookbook/hud-overlay).

## Performance

Keep an eye on how much of the page repaints:

- do not animate the whole screen for a tiny status change;
- cache DOM lookups instead of calling `querySelector` every frame;
- batch text updates and avoid layout thrash;
- use `requestAnimationFrame` only for real animation;
- prefer CSS transitions over JavaScript animation loops;
- keep persistent HUDs mostly static.

A PrismaUI page renders into the game frame. It is not free, and a heavy page will show up as frame cost.

## Where to go next

- [Your first panel](quick-start) if you skipped it.
- [Cookbook](/docs/cookbook/) for complete, working examples.
- [View lifecycle](view-lifecycle) for what happens when the page reloads.
- [Translations](translations) to localize the page.
- [Networking](networking) when your mod needs the internet.
- [Troubleshooting](troubleshooting) when something in the page does not behave.
