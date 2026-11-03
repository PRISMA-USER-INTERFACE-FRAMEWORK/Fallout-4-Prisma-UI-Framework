---
title: Build a settings page
sidebar_label: Settings page
sidebar_position: 9
---

# Build a settings page

This builds a real settings screen: sliders and toggles that live in your page, values sent to the plugin as JSON, and values loaded back when the panel opens.

PrismaUI is the UI layer; saving the values to disk is your plugin's job. The recipe shows the boundary and a simple way to persist.

## 1. The page

```html
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>Settings</title>
  <link rel="stylesheet" href="settings.css">
</head>
<body>
  <main class="panel">
    <h1>SETTINGS</h1>

    <label class="row">
      <span>HUD scale</span>
      <input id="scale" type="range" min="0.5" max="2" step="0.1" value="1">
      <output id="scale-out">1.0</output>
    </label>

    <label class="row">
      <span>Show compass</span>
      <input id="compass" type="checkbox" checked>
    </label>

    <label class="row">
      <span>Crosshair style</span>
      <select id="crosshair">
        <option value="default">Default</option>
        <option value="dot">Dot</option>
        <option value="none">Hidden</option>
      </select>
    </label>

    <p id="status"></p>
    <div class="actions">
      <button id="save" type="button">Save</button>
      <button id="close" type="button">Close</button>
    </div>
  </main>
  <script src="settings.js"></script>
</body>
</html>
```

```css
html, body { margin: 0; width: 100%; height: 100%; background: transparent; font-family: "Courier New", monospace; color: #d4d4d4; }
.panel { width: 460px; margin: 12vh auto 0; padding: 20px; background: rgba(8, 12, 10, 0.96); border: 1px solid #3d5a45; }
h1 { margin: 0 0 16px; font-size: 18px; letter-spacing: 2px; }
.row { display: flex; align-items: center; justify-content: space-between; gap: 12px; padding: 8px 0; }
input[type="range"] { width: 180px; }
.actions { display: flex; gap: 10px; margin-top: 18px; }
button { padding: 6px 14px; background: #1b2b20; color: inherit; border: 1px solid #3d5a45; font: inherit; cursor: pointer; }
#status { min-height: 18px; color: #8fbf9a; font-size: 12px; }
```

```js
const scale = document.getElementById('scale');
const scaleOut = document.getElementById('scale-out');
const compass = document.getElementById('compass');
const crosshair = document.getElementById('crosshair');
const status = document.getElementById('status');

function currentValues() {
  return {
    scale: Number(scale.value),
    compass: compass.checked,
    crosshair: crosshair.value,
  };
}

function render(values) {
  scale.value = String(values.scale ?? 1);
  scaleOut.textContent = Number(scale.value).toFixed(1);
  compass.checked = values.compass !== false;
  crosshair.value = values.crosshair || 'default';
}

scale.addEventListener('input', () => {
  scaleOut.textContent = Number(scale.value).toFixed(1);
});

document.getElementById('save').addEventListener('click', () => {
  // Calls the C++ listener registered for "saveSettings".
  window.saveSettings(JSON.stringify(currentValues()));
  status.textContent = 'Saved.';
});

document.getElementById('close').addEventListener('click', () => {
  window.requestClose();
});

// The plugin calls this with the stored settings when the panel opens.
window.setSettings = function (json) {
  try {
    render(JSON.parse(json));
    status.textContent = '';
  } catch (e) {
    console.error('bad settings payload', e);
  }
};
```

## 2. The plugin

```cpp
#include "PrismaUI_F4_API.h"
#include <fstream>
#include <string>

static PRISMA_UI_API::IVPrismaUI10* g_api  = nullptr;
static PrismaView                   g_view = 0;

static const char* kSettingsPath = "Data\\PrismaUI_F4\\MyPlugin.ini";   // your choice

static std::string LoadSettingsJson()
{
    std::ifstream file(kSettingsPath);
    if (!file) return R"({"scale":1,"compass":true,"crosshair":"default"})";

    std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return json.empty() ? R"({"scale":1,"compass":true,"crosshair":"default"})" : json;
}

static void SaveSettingsJson(const char* json)
{
    if (!json) return;
    std::ofstream file(kSettingsPath, std::ios::trunc);
    if (!file) {
        REX::WARN("could not write settings file");
        return;
    }
    file << json;
}

static void OnDomReady(PrismaView view)
{
    // Page -> plugin: the Save button.
    g_api->BindUIEvent(view, "saveSettings", [](const char* json) {
        SaveSettingsJson(json);
        // Apply the values to the game here (HUD scale, compass visibility, ...).
    });

    // Plugin -> page: close request and initial values.
    g_api->BindUIEvent(view, "requestClose", [](const char*) {
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
    });

    const std::string settings = LoadSettingsJson();
    g_api->InteropCall(view, "setSettings", settings.c_str());
}

static void CreateView()
{
    if (!g_api || (g_view && g_api->IsValid(g_view))) return;

    g_view = g_api->CreateView("MyPlugin/settings.html", OnDomReady);
    if (!g_view) return;

    g_api->SetViewRole(g_view, PRISMA_UI_API::ViewRole::kPanel);
    g_api->Hide(g_view);
}

static void OpenSettings()
{
    if (!g_api || !g_api->IsValid(g_view)) return;
    if (g_api->IsAnyPanelVisible(g_view)) return;

    g_api->Show(g_view);
    g_api->Focus(g_view, /*pauseGame=*/true);
}
```

## 3. Test it

1. Open the panel. The page shows the values from the last save, or the defaults on first run.
2. Move the slider, toggle the checkbox, change the select.
3. Click **Save**. The JSON is written and applied.
4. Close, reopen: the saved values come back.
5. Click **Close** without saving: the page still shows the last saved values on the next open.

## Notes

- **The page owns presentation, the plugin owns truth.** Load the stored values in `OnDomReady` and push them into the page; do not let the page invent defaults the plugin does not know about.
- **`InteropCall` needs the page function to exist.** `setSettings` is defined at module load in this example, so calling it from `OnDomReady` is safe.
- **Writes are fire-and-forget from the page.** If saving can fail, have the plugin call back (`window.setStatus('save failed')`) so the player knows.
- **Store and apply are separate concerns.** Keeping the JSON save and the game-apply steps in their own helpers makes it easy to reset, migrate, or add a "defaults" button later.
- **Respect other panels.** `IsAnyPanelVisible` keeps you from stacking on another mod's menu.

## Where to go next

- [Talk between the page and the plugin](talk-to-your-plugin) — the messaging basics
- [The HTML side](../html-views) — layout and page rules
- [Panel management](../panel-management) — roles and focus
- [Packaging your mod](../packaging) — where the settings file should live
