---
id: networking
title: Networking
sidebar_label: Networking
sidebar_position: 10
---

:::tip New to PrismaUI?
Start with [Your first panel](quick-start) for a complete walkthrough, or copy a working example from the [Cookbook](/docs/cookbook/).
:::

# Networking

The page inside a PrismaUI view is a local document. It can load its own HTML, CSS, JavaScript, fonts and images, and that is all. It is not a web browser and it cannot talk to the internet.

That is deliberate, and it shapes how you build anything online.

## The rule

```
internet  ──►  your C++ plugin  ──►  InteropCall / Invoke  ──►  the page
                  (network, TLS,            JSON
                   keys, retries)
```

- **Networking happens in C++.** HTTP, WebSockets, authentication, retries, TLS — your plugin does it.
- **The page only displays the result.** Your plugin sends validated JSON with `InteropCall`; the page renders it.
- **Nothing required by the UI lives on a CDN.** No Google Fonts, no jsDelivr, no remote scripts or styles.

If your mod loads a remote resource for the production view, it will fail for some players. The framework blocks it before the page sees it.

## What is blocked in the page

These do not work from view JavaScript:

- remote `fetch()` and `XMLHttpRequest`;
- `WebSocket`, `EventSource`, `WebTransport`;
- `Worker`, `SharedWorker`, service workers;
- WebRTC (`RTCPeerConnection`);
- remote scripts, stylesheets and fonts;
- `file://` paths outside your view folder;
- localhost and private-network addresses.

`data:` URLs and local files inside your view folder are fine.

## The pattern that works

### 1. Do the network work off the game thread

```cpp
static void FetchPlayerStats()
{
    // Your own worker thread or async client. Never block the game thread on I/O.
    std::thread([] {
        std::string json = MyHttpClient::Get("https://api.example.com/stats");
        // Validate and shape the data before it reaches the UI.
        auto result = BuildStatsPayload(json);
        OnNetworkResult(std::move(result));
    }).detach();
}
```

### 2. Hand the result back to the game thread

```cpp
static void OnNetworkResult(std::string json)
{
    F4SE::GetTaskInterface()->AddTask([json = std::move(json)]() {
        if (!g_api || !g_api->IsValid(g_view)) return;   // the view may be gone
        g_api->InteropCall(g_view, "onStats", json.c_str());
    });
}
```

The `IsValid` check matters: the player can close the panel while your request is in flight.

### 3. Render it in the page

```js
window.onStats = function (json) {
  try {
    const stats = JSON.parse(json);
    document.getElementById('level').textContent = stats.level;
    document.getElementById('hours').textContent = stats.hours;
  } catch (e) {
    console.error('bad stats payload', e);
  }
};
```

That is the whole contract: native fetch, game-thread handoff, JSON into the page.

## Local assets

Keep everything the page needs in its view folder:

```text
Data/PrismaUI_F4/views/MyPlugin/
├── index.html
├── app.js
├── app.css
├── fonts/
│   └── interface.woff2
└── images/
    └── icon.webp
```

```html
<link rel="stylesheet" href="app.css">
<script src="app.js"></script>
```

```css
@font-face {
  font-family: 'MyInterface';
  src: url('fonts/interface.woff2') format('woff2');
}
```

A mod that works offline works for every player, on every connection, and after the remote service changes its URLs.

## If your plugin needs localhost or a LAN service

Do it in native code, where you control the address, protocol, validation and error handling. Do not try to reach loopback from page JavaScript — the sandbox blocks it, and working around it is not a supported pattern.

## Debugging a blank page caused by a remote asset

1. Look at `PrismaUI_F4.log` — CSP refusals are logged.
2. Open F12 and check the Console and Network tabs.
3. Search the page for `http://`, `https://`, `//cdn`, `fonts.googleapis`.
4. Move that file into the view folder and point at it relatively.
5. Test with the machine offline: the interface must still render fully.

## Where to go next

- [The HTML side](html-views) — the full page contract
- [Limitations](limitations) — every unsupported web feature in one list
- [Debugging your view](debugging) — when a page loads but looks wrong
- [Packaging your mod](packaging) — shipping local assets
