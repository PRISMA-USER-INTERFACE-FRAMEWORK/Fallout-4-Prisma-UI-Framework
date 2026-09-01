# Writing HTML Views for PrismaUI_F4 2.1.0

## Runtime model

PrismaUI_F4 2.1.0 renders views with **Ultralight 1.4.0 in-process**. It is not previous browser runtime/legacy-runtime, so do not assume that every API supported by a current desktop Chrome build exists in-game.

Author views as portable HTML/CSS/JavaScript, bundle required dependencies locally, and feature-detect browser APIs that are not part of your tested baseline.

## File location

Give each plugin its own directory:

| Source example | Installed/MO2 virtual path |
|---|---|
| `YourPlugin/view/index.html` | `Data/PrismaUI_F4/views/YourPlugin/index.html` |
| `YourPlugin/view/app.css` | `Data/PrismaUI_F4/views/YourPlugin/app.css` |
| `YourPlugin/view/app.js` | `Data/PrismaUI_F4/views/YourPlugin/app.js` |

Then create the page with the path relative to the Prisma views root:

```cpp
view = api->CreateView("YourPlugin/index.html", OnDomReady);
```

Do not pass an arbitrary absolute `file://` URL. Prisma local content is confined by the framework's view-path policy.

## Keep required assets local

Recommended layout:

```text
Data/PrismaUI_F4/views/YourPlugin/
├── index.html
├── app.css
├── app.js
├── fonts/
│   └── ui.woff2
└── images/
    └── icon.webp
```

HTML:

```html
<link rel="stylesheet" href="app.css">
<script src="app.js"></script>
```

Font:

```css
@font-face {
  font-family: 'MyUI';
  src: url('fonts/ui.woff2') format('woff2');
}
```

Do not make a required UI path depend on Google Fonts, jsDelivr, another CDN, or an external server.

## JavaScript expectations

Normal page scripting is supported, including the DOM, events, timers, Promises, JSON, and the JavaScript features provided by Ultralight 1.4's JavaScript runtime.

Do not document PrismaUI 2.1.0 as “full current previous browser runtime.” Feature-detect optional APIs:

```js
if (typeof ResizeObserver !== 'undefined') {
  const observer = new ResizeObserver(() => updateLayout());
  observer.observe(document.body);
}
```

Worker/network-oriented browser APIs are intentionally restricted by the framework. See [Networking](networking).

## Network restrictions

The page is not a general-purpose network client.

In the shipping 2.1.0 policy:

- `WebSocket`, `EventSource`, `Worker`, `SharedWorker`, `WebTransport`, and `RTCPeerConnection` are blocked from normal view code;
- `sendBeacon` and service-worker use are disabled;
- arbitrary `file://` content access is not supported;
- general `fetch`/XHR connect access is not available to arbitrary external hosts;
- local/private-network targets remain protected;
- required scripts, styles, fonts, and images should be local to the mod.

If the mod requires HTTP, WebSocket, authentication, or server push, do that in native C++ and pass validated data to the page through the Prisma bridge.

## C++ to JavaScript

For a named call:

```cpp
api->InteropCall(view, "onPlayerData", R"({"hp":210,"ap":75})");
```

```js
window.onPlayerData = function (json) {
  const data = JSON.parse(json);
  document.querySelector('#hp').textContent = String(data.hp);
};
```

For arbitrary evaluated JavaScript:

```cpp
api->Invoke(view, "window.refresh && window.refresh()");
```

Prefer `InteropCall` for normal structured application messages. Pass JSON strings for structured data.

## JavaScript to C++

Register listeners in the view's DOM-ready path:

```cpp
static void OnCloseRequested(const char*)
{
    if (!g_api || !g_api->IsValid(g_view)) return;
    g_api->Unfocus(g_view);
    g_api->Hide(g_view);
}

static void OnDomReady(PrismaView view)
{
    g_api->RegisterJSListener(view, "requestClose", OnCloseRequested);
}
```

```js
document.querySelector('#close').addEventListener('click', () => {
  window.requestClose();
});
```

In 2.1.0, callbacks delivered through both `RegisterJSListener` and `BindUIEvent` are marshalled onto the game thread. Choose between them for their API/event semantics, not for thread selection. Do not add another `F4SE::GetTaskInterface()->AddTask` solely because a Prisma callback originated in JavaScript.

## Console logging

Use normal JavaScript console calls:

```js
console.log('view loaded');
console.warn('unexpected value');
console.error('operation failed');
```

Capture them in C++:

```cpp
static void OnConsole(
    PrismaView,
    PRISMA_UI_API::ConsoleMessageLevel level,
    const char* message)
{
    logger::info("[JS {}] {}", static_cast<int>(level), message ? message : "");
}

api->RegisterConsoleCallback(view, OnConsole);
```

The Ultralight 2.1.0 backend does **not** provide the old legacy-runtime inspector/Chrome DevTools workflow. The V1 inspector calls remain ABI compatibility methods but are unsupported.

## Layout

Normal on-screen views are laid out against the game presentation surface. Use responsive CSS rather than assuming one resolution:

```css
html, body {
  width: 100%;
  height: 100%;
  margin: 0;
}

.panel {
  width: min(700px, 90vw);
  margin: 0 auto;
}
```

Avoid hard-coded coordinates that only work at 1920x1080.

## Transparency

For a floating overlay:

```css
body {
  background: transparent;
}

.panel {
  background: rgba(8, 8, 8, 0.9);
}
```

For advanced offscreen/mesh use, V9 exposes `SetViewOffscreenBackground`. That path is separate from normal on-screen transparency and should be tested with the exact integration you ship.

## Focus and input

Showing a view does not by itself make it an interactive panel.

```cpp
api->SetViewRole(view, PRISMA_UI_API::ViewRole::kPanel);
api->Show(view);
api->Focus(view, false, false);
```

For V10 selective input, capability-check `PrismaCapability::InputRegions` and use `SetInputRegions` + `FocusOverlay`.

Do not try to solve engine input capture purely with CSS `pointer-events`. Prisma focus/input ownership is a native framework concern.

## Performance on the shipping renderer

The public 2.1.0 renderer uses the CPU BitmapSurface path. Performance therefore depends on how much content changes and how much surface area must be copied/composited.

Good practices:

- keep persistent HUDs mostly static;
- animate the smallest practical region;
- avoid repainting a full-screen surface for a tiny indicator change;
- cache DOM references;
- batch DOM updates;
- avoid unnecessary large blurs/filters;
- use `requestAnimationFrame` only for actual animation work;
- use a lower offscreen resolution for mesh-oriented views when appropriate.

Do not repeat the retired legacy-runtime advice that page work happens in an independent renderer subprocess and therefore cannot affect game-frame cost.

## Storage and persistence

Do not use browser storage as the authoritative persistence layer for game/mod state. Persist important state through your native plugin, Papyrus/save-backed data, or another explicit mod storage mechanism.

Treat browser storage as optional UI convenience data only.

## Minimal page

```html
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>My Prisma UI</title>
  <link rel="stylesheet" href="app.css">
</head>
<body>
  <main class="panel">
    <h1 id="title">MENU</h1>
    <button id="close" type="button">Close</button>
  </main>
  <script src="app.js"></script>
</body>
</html>
```

```js
window.setTitle = function (title) {
  document.querySelector('#title').textContent = title;
};

document.querySelector('#close').addEventListener('click', () => {
  window.requestClose();
});
```

## Related pages

- [Quick Start](quick-start)
- [View Lifecycle](view-lifecycle)
- [Networking](networking)
- [Translations](translations)
- [Model Preview](model-preview)
- [Limitations](limitations)
- [Troubleshooting](troubleshooting)
