---
title: Talk between the page and the plugin
sidebar_label: Page ↔ plugin messages
sidebar_position: 2
---

# Talk between the page and the plugin

This adds a working two-way conversation to the panel from [Your first panel](../quick-start): the plugin sends JSON to the page, and the page sends JSON back.

You will use:

- `Invoke` — run one line of JavaScript in the page.
- `InteropCall` — call a named JavaScript function with a payload.
- `BindUIEvent` — call a C++ function when the page calls a named function.

## 1. The page

Replace `index.html` with:

```html
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>Two-way</title>
  <link rel="stylesheet" href="app.css">
</head>
<body>
  <main class="panel">
    <h1>Two-way</h1>
    <p>Health: <span id="hp">?</span></p>
    <p>Status: <span id="status">waiting</span></p>
    <button id="heal" type="button">Send "heal" to the plugin</button>
    <button id="close" type="button">Close</button>
  </main>
  <script src="app.js"></script>
</body>
</html>
```

and `app.js` with:

```js
// The plugin calls this to update the page.
window.onPlayerData = function (json) {
  const data = JSON.parse(json);
  document.getElementById('hp').textContent = data.hp;
  document.getElementById('status').textContent = 'updated ' + data.time;
};

// The plugin calls this for short one-line messages.
window.setStatus = function (text) {
  document.getElementById('status').textContent = text;
};

// Ask the plugin to heal the player.
document.getElementById('heal').addEventListener('click', function () {
  window.sendCommand(JSON.stringify({ cmd: 'heal', amount: 25 }));
});

// Ask the plugin to close the panel.
document.getElementById('close').addEventListener('click', function () {
  window.requestClose();
});
```

## 2. The plugin

In `OnDomReady`, register what the page can call. Register the console callback first so JavaScript errors reach your log.

```cpp
static void OnDomReady(PrismaView view)
{
    g_api->RegisterConsoleCallback(view, [](PrismaView, PRISMA_UI_API::ConsoleMessageLevel level, const char* message) {
        switch (level) {
        case PRISMA_UI_API::ConsoleMessageLevel::Error:   REX::CRITICAL("[JS] {}", message); break;
        case PRISMA_UI_API::ConsoleMessageLevel::Warning: REX::WARN("[JS] {}", message); break;
        default:                                          REX::INFO("[JS] {}", message); break;
        }
    });

    g_api->BindUIEvent(view, "sendCommand", [](const char* json) {
        if (!json) return;
        REX::INFO("page asked for: {}", json);

        // Do real work here: heal the player, change a setting, start a quest.
        // The callback already runs on the game thread.

        // Answer the page.
        g_api->InteropCall(g_view, "setStatus", "healed 25 hp");
    });

    g_api->BindUIEvent(view, "requestClose", [](const char*) {
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
    });

    // Send the first data set as soon as the page exists.
    SendPlayerData();
}
```

and send data whenever it changes:

```cpp
static void SendPlayerData()
{
    if (!g_api || !g_api->IsValid(g_view)) return;

    char payload[128];
    snprintf(payload, sizeof(payload), R"({"hp":%d,"time":"%s"})", GetPlayerHealth(), GetClockText());
    g_api->InteropCall(g_view, "onPlayerData", payload);
}
```

`GetPlayerHealth()` and `GetClockText()` are your own helpers. When the player is healed, call `SendPlayerData()` again.

## 3. Test it

1. Build and copy the DLL and the view folder as usual.
2. Open the panel. The status line goes from "waiting" to `updated <time>` as soon as the page is ready.
3. Click **Send "heal"**. The plugin log shows the JSON it received, and the page status changes to "healed 25 hp".
4. Click **Close**, or press your F3 key, and the panel hides.

## Notes

- **Register listeners in `OnDomReady`.** Calls the page makes before that are lost.
- **Keep function names identical.** `BindUIEvent("sendCommand", ...)` pairs with `window.sendCommand(...)`.
- **Pass JSON as a string.** Both directions take a `const char*`; the page parses it with `JSON.parse`.
- **Callbacks already run on the game thread.** Read and write game state directly; do not add another task hop.
- **Guard before sending.** `IsValid(g_view)` keeps a late timer or event from calling into a destroyed view.
- **Errors are silent by default.** The console callback in step 2 is what turns a JavaScript `TypeError` into a log line.

## Where to go next

- [A click-through HUD overlay](hud-overlay)
- [Translate your interface](translate-your-ui)
- [View lifecycle](../view-lifecycle) for what happens when the page reloads
