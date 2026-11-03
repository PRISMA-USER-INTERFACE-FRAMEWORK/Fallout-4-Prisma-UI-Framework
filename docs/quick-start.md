---
id: quick-start
title: Your first panel
sidebar_label: Your first panel
sidebar_position: 2
---

# Your first panel

This builds a small F4SE plugin that shows a panel when you press **F3**. It takes about ten minutes. You need a working F4SE plugin build setup, because this is real C++ — if you have never built one, do [Install PrismaUI](getting-started) first, then come back.

You will end up with two halves:

- a **page** in `Data/PrismaUI_F4/views/MyPlugin/`, which is plain HTML and JavaScript;
- a **plugin DLL** in `Data/F4SE/Plugins/`, which creates and shows the page.

## What you need first

- PrismaUI installed and working. Start the game once and check that `Documents\My Games\Fallout4\F4SE\PrismaUI_F4.log` exists.
- A C++ plugin project that already builds and loads. The [example plugin](https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Fallout-4-Prisma-UI-Framework/tree/main/example-f4se-plugin) is the same pattern used below — copy it if you want a starting point.
- The public header `PrismaUI_F4_API.h` copied into your project.

## 1. Make the page

Create this folder and two files:

```text
Data/PrismaUI_F4/views/MyPlugin/
├── index.html
└── app.css
```

`index.html`:

```html
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>My Panel</title>
  <link rel="stylesheet" href="app.css">
</head>
<body>
  <main class="panel">
    <h1>My Panel</h1>
    <p id="status">Waiting for the plugin…</p>
    <button id="close" type="button">Close</button>
  </main>
  <script src="app.js"></script>
</body>
</html>
```

`app.css` — this gives it a Fallout-ish look and a transparent page background so you do not get a white rectangle over the game:

```css
html, body {
  margin: 0;
  width: 100%;
  height: 100%;
  background: transparent;
  font-family: "Courier New", monospace;
  color: #d4d4d4;
}

.panel {
  width: 420px;
  margin: 15vh auto 0;
  padding: 18px 20px;
  background: rgba(8, 12, 10, 0.94);
  border: 1px solid #3d5a45;
}

h1 { margin: 0 0 10px; font-size: 20px; letter-spacing: 2px; }

button {
  margin-top: 14px;
  padding: 6px 14px;
  background: #1b2b20;
  color: #d4d4d4;
  border: 1px solid #3d5a45;
  font: inherit;
  cursor: pointer;
}

button:hover { background: #27402f; }
```

`app.js` — one function the plugin can call, and one button that asks the plugin to close the panel:

```js
// The plugin calls this through Invoke().
window.setStatus = function (text) {
  document.getElementById('status').textContent = text;
};

// The button calls a function the plugin registered with BindUIEvent().
document.getElementById('close').addEventListener('click', function () {
  window.requestClose();
});
```

## 2. Create the view from your plugin

Add this to your plugin's source file. It uses the stable numbered API, which is the shortest complete path. The modern feature tables do the same job — see [Modern API](modern-api) once this works.

```cpp
#include "PrismaUI_F4_API.h"

static PRISMA_UI_API::IVPrismaUI10* g_api  = nullptr;
static PrismaView                   g_view = 0;
static bool                         g_visible = false;

// PrismaUI calls this once the page is loaded and its DOM is ready.
static void OnDomReady(PrismaView view)
{
    // Tell JavaScript we are alive.
    g_api->Invoke(view, "window.setStatus('Plugin connected.')");

    // Register the function the page calls from its Close button.
    g_api->BindUIEvent(view, "requestClose", [](const char*) {
        g_visible = false;
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
    });
}

static void CreateView()
{
    if (!g_api) return;

    g_view = g_api->CreateView("MyPlugin/index.html", OnDomReady);
    if (!g_view) {
        REX::CRITICAL("could not create the PrismaUI view");
        return;
    }

    // Tell other PrismaUI mods this is an interactive panel.
    g_api->SetViewRole(g_view, PRISMA_UI_API::ViewRole::kPanel);

    // Views start visible. Hide it until the player opens it.
    g_api->Hide(g_view);
}

static void TogglePanel()
{
    if (!g_api || !g_api->IsValid(g_view)) return;

    g_visible = !g_visible;
    if (g_visible) {
        g_api->Show(g_view);
        g_api->Focus(g_view, /*pauseGame=*/false);
    } else {
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
    }
}
```

If you already have a key handler from the example plugin, use it. If not, the simplest option is the key sink from the example (`KeyHandler::RegisterSink()` plus `Register(BS_BUTTON_CODE::kF3, KEY_DOWN, TogglePanel)`). Any input source works — Papyrus, a keybind, a game event — as long as you call `TogglePanel` on the game thread.

## 3. Hook it up when the game is ready

```cpp
static void OnMessage(F4SE::MessagingInterface::Message* message)
{
    switch (message->type) {
    case F4SE::MessagingInterface::kGameDataReady:
        // PrismaUI is loaded by now. Ask for the interface once.
        g_api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI10>();
        if (!g_api) {
            REX::CRITICAL("PrismaUI_F4 is not installed");
            return;
        }
        // Register your F3 key here, as in the example plugin.
        break;

    case F4SE::MessagingInterface::kPostLoadGame:
    case F4SE::MessagingInterface::kNewGame:
        // The game world exists; the page can be created now.
        CreateView();
        break;

    default:
        break;
    }
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_intfc)
{
    F4SE::Init(a_intfc);
    F4SE::GetMessagingInterface()->RegisterListener(OnMessage);
    return true;
}
```

Why this order: PrismaUI's API becomes available at `kGameDataReady`, and the game is only ready for a UI view after a save or a new game. Creating the view earlier gives you a handle that cannot be shown.

## 4. Build and install

1. Build your plugin as usual and copy the DLL to `Data\F4SE\Plugins\`.
2. Copy the `Data\PrismaUI_F4\views\MyPlugin\` folder into the game's `Data\PrismaUI_F4\views\`.
3. Start the game, load a save, press **F3**.

You should see the panel with "Plugin connected." and a working Close button.

If you use Mod Organizer 2, keep the same layout inside the mod folder; both paths above are virtual paths under `Data`.

## If it does not show up

Check `Documents\My Games\Fallout4\F4SE\PrismaUI_F4.log` first. Common causes:

- **The log says the view path is invalid** — the folder name in `CreateView` must match the folder under `views` exactly, including capital letters.
- **The plugin logged "PrismaUI_F4 is not installed"** — the request happened before `kGameDataReady`, or the framework DLL is not loaded.
- **The panel appears as a white box** — `app.css` is missing or not linked from `index.html`.
- **F3 does nothing** — your key handler is not registered, or `CreateView` was never called after load.
- **The panel opens but cannot be clicked** — check that `Focus` is called with a valid view and that `SetViewRole(..., kPanel)` ran.

More in [Troubleshooting](troubleshooting).

## What each part does

- `CreateView(path, OnDomReady)` loads the page and returns a handle. The path is relative to `Data/PrismaUI_F4/views/`.
- `OnDomReady` is where you set up everything that needs the page to exist: event bindings, initial data, controller actions, translations.
- `Show` / `Hide` control whether the panel is drawn. `Focus` / `Unfocus` control whether it takes mouse and keyboard input.
- `Invoke` runs a JavaScript string in the page. `BindUIEvent` calls C++ when the page calls a named function.
- `SetViewRole` lets other PrismaUI mods know what kind of view this is.

## Where to go next

- [The HTML side](html-views) — how the page and the plugin talk, and what web features are available.
- [Cookbook](/docs/cookbook/) — complete examples for a HUD overlay, item previews, controller menus and translations.
- [View lifecycle](view-lifecycle) — what happens when a view is created, reloaded and destroyed.
- [Modern API](modern-api) — the preferred API for new code.
