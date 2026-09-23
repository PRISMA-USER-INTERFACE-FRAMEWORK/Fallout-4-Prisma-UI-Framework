---
id: networking
title: Networking
sidebar_label: Networking
sidebar_position: 10
---

# Networking in PrismaUI_F4 2.1.0

PrismaUI_F4 2.1.0 uses an in-process **Ultralight 1.4.0** backend with a deliberately restrictive content-security policy. A Prisma view should be treated as a **local UI document**, not as a general-purpose web client.

If your mod needs HTTP, WebSocket, server push, authentication, or other real network behavior, do that work in native C++ and pass the result into the view through `InteropCall`, `Invoke`, or your registered bridge callbacks.

## Shipping 2.1.0 rule

For the production Ultralight path, required application assets should be shipped with the mod.

Use:

- local HTML/CSS/JS under `Data/PrismaUI_F4/views/`
- local images and fonts bundled with the mod
- `data:` / `blob:` only where the framework CSP permits them
- C++ for external network I/O

Do **not** build a required UI path around:

- remote `fetch()` or XHR
- CDN-hosted JavaScript or CSS
- Google Fonts or other remote fonts
- WebSocket / EventSource
- Worker / SharedWorker / ServiceWorker
- arbitrary `file://` URLs
- localhost, LAN, router, or other private-network access

Older earlier-runtime documentation described a remote-domain allowlist. That is **not the production Ultralight 2.1.0 contract**. The Ultralight CSP is intentionally stricter and does not add those external hosts to the shipping transport policy.

## Why the restriction exists

A mod UI runs inside the game process. Allowing arbitrary HTML to make unrestricted requests would turn every installed UI asset into a potential network client with access to the player's environment.

Prisma therefore separates responsibilities:

```text
Network / external service
        |
        v
Your native F4SE plugin (C++)
        |
        | validated data / JSON
        v
InteropCall / Invoke / UI event bridge
        |
        v
PrismaUI local view
```

This keeps network policy, credentials, TLS libraries, threading, retries, and validation in code you control instead of depending on browser behavior.

## Recommended pattern

### C++ side

Run blocking or long-lived network work away from the game thread. When data is ready, marshal the Prisma call onto the game thread before touching the UI.

```cpp
static PRISMA_UI_API::IVPrismaUI2* g_api = nullptr;
static PrismaView g_view = 0;

void OnNetworkResult(std::string json)
{
    F4SE::GetTaskInterface()->AddTask([json = std::move(json)]() {
        if (!g_api || !g_view || !g_api->IsValid(g_view)) {
            return;
        }
        g_api->InteropCall(g_view, "onNetworkResult", json.c_str());
    });
}
```

### JavaScript side

```js
window.onNetworkResult = function (json) {
  const data = JSON.parse(json);
  render(data);
};
```

The view never needs direct access to the remote service.

## Local assets

Keep dependencies inside your mod whenever possible. A typical layout is:

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

Then reference them with document-relative URLs:

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

This is more reliable for players, works offline, avoids third-party availability changes, and matches the 2.1.0 security model.

## Private/local network protection

Prisma also maintains native and script-level protections around loopback/private network targets. Mod authors should not try to work around them from JavaScript.

If your own plugin intentionally needs localhost or LAN communication, implement that connection in native code where you can explicitly define the address, protocol, validation, and user expectations.

## Threading

Do not block the game thread for network I/O.

A safe pattern is:

1. perform HTTP/socket work on a worker thread or asynchronous native client;
2. validate and parse the response;
3. queue a task back to the game thread;
4. verify the Prisma view still exists;
5. call `InteropCall` or `Invoke`.

## Debugging

If a page tries to load a remote dependency and the UI is blank or incomplete:

1. confirm the asset is not hosted on a CDN;
2. move the dependency into your mod's view directory;
3. check `PrismaUI_F4.log` and your plugin log for load/console errors;
4. register `RegisterConsoleCallback` during development;
5. verify the page works with all external network access disabled.

For release-quality PrismaUI content, an offline machine should still be able to render the complete interface.

## Summary

- PrismaUI_F4 2.1.0 views are local UI documents.
- Self-host required HTML, CSS, JS, fonts, and images with the mod.
- Do external HTTP/WebSocket work in C++.
- Push validated results into the view through the Prisma bridge.
- Do not rely on the old legacy-runtime remote-domain whitelist documentation.
- Do not attempt to bypass private-network or arbitrary-file restrictions from JavaScript.
