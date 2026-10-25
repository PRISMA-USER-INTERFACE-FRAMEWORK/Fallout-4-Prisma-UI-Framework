---
title: A click-through HUD overlay
sidebar_label: Click-through HUD
sidebar_position: 3
---

# A click-through HUD overlay

This puts a small HUD on screen that stays out of the way: the game keeps receiving mouse input everywhere except the two buttons of your HUD.

This is the V10 input-region path. On a framework build without input regions, the code falls back to a normal focusable panel.

## 1. The page

The HUD is two rows in the top-left corner. The rest of the page is transparent and ignores clicks.

```html
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>HUD</title>
  <link rel="stylesheet" href="hud.css">
</head>
<body>
  <div class="hud">
    <button id="scan" type="button">Scan area</button>
    <button id="map" type="button">Open map</button>
  </div>
  <script src="hud.js"></script>
</body>
</html>
```

```css
html, body {
  margin: 0;
  width: 100%;
  height: 100%;
  background: transparent;
  overflow: hidden;
  font-family: "Courier New", monospace;
}

.hud {
  position: absolute;
  top: 40px;
  left: 40px;
  display: flex;
  flex-direction: column;
  gap: 10px;
}

.hud button {
  width: 180px;
  height: 48px;
  background: rgba(8, 14, 10, 0.9);
  color: #d6e8d9;
  border: 1px solid #3d5a45;
  font: inherit;
  cursor: pointer;
}
```

```js
document.getElementById('scan').addEventListener('click', function () {
  window.scanArea();
});

document.getElementById('map').addEventListener('click', function () {
  window.openMap();
});

// The plugin can also call this to flash a message on the HUD.
window.showHint = function (text) {
  console.log('hint: ' + text);
};
```

## 2. The plugin

The important part is `SetInputRegions`: it tells PrismaUI which rectangles on screen belong to you. Everything else passes through to the game.

```cpp
#include "PrismaUI_F4_API.h"

static PRISMA_UI_API::IVPrismaUI10* g_api  = nullptr;
static PrismaView                   g_hud  = 0;

static void OnDomReady(PrismaView view)
{
    g_api->BindUIEvent(view, "scanArea", [](const char*) {
        // Do the scan. The callback is already on the game thread.
        g_api->Invoke(g_hud, "showHint('scanning')");
    });

    g_api->BindUIEvent(view, "openMap", [](const char*) {
        // Open your map panel.
    });
}

// The two interaction rectangles, in screen pixels. They must match where
// the HTML buttons are drawn, including at other resolutions.
static void UpdateInputRegions()
{
    if (!g_api || !g_api->IsValid(g_hud)) return;

    PRISMA_UI_API::InputRegion regions[2] = {
        { 40, 40, 180, 48 },   // Scan area
        { 40, 98, 180, 48 },   // Open map
    };

    if (!g_api->SetInputRegions(g_hud, regions, 2)) {
        REX::WARN("this PrismaUI build does not support input regions");
    }
}

static void CreateHud()
{
    if (!g_api) return;

    g_hud = g_api->CreateView("MyPlugin/hud.html", OnDomReady);
    if (!g_hud) return;

    g_api->SetViewRole(g_hud, PRISMA_UI_API::ViewRole::kPanel);

    if (PRISMA_UI_API::HasPrismaCapability(
            PRISMA_UI_API::PrismaCapability::InputRegions)) {
        UpdateInputRegions();
        g_api->Show(g_hud);
        // false = do not pause the game, true = no focus menu
        g_api->FocusOverlay(g_hud, false, true);
    } else {
        // Older framework: behave as a normal focusable panel.
        g_api->Show(g_hud);
        g_api->Focus(g_hud, false);
    }
}
```

Call `CreateHud()` from `kPostLoadGame` and `kNewGame`, like the panel in [Your first panel](../quick-start).

If your HUD moves (a settings slider, a resolution change, one of several anchors), call `UpdateInputRegions()` again after the page relayouts. If the rectangles do not match the buttons, clicks go to the game instead of your HUD.

## 3. Hide the HUD when the player does not need it

```cpp
static void HideHud()
{
    if (!g_api || !g_api->IsValid(g_hud)) return;
    g_api->Unfocus(g_hud);
    g_api->Hide(g_hud);
}
```

Show it again later with `Show` + `FocusOverlay` (re-apply the regions after `Show` if the page re-rendered).

## Notes

- **Region coordinates are screen pixels**, not page CSS pixels — but since a PrismaUI view spans the game window at 1:1, `getBoundingClientRect()` on the page gives the same values at the default scale. Recompute if you scale the page with CSS transforms.
- **Keep overlays small.** A full-screen overlay with one input region still costs you a full-screen page.
- **Only the region takes clicks.** Everything else reaches the game, which is what makes this feel like a real HUD and not a modal.
- **The game must not be paused.** `FocusOverlay(hud, false, true)` keeps the game running while the HUD owns its small input area.
- **Mode changes matter.** When the player enters a menu, hide the HUD — a PrismaUI view stays above the game even in menus.

## Where to go next

- [A controller-friendly menu](controller-menu)
- [The HTML side](../html-views) for layout and performance tips
- [API reference: FocusOverlay](../api/FocusOverlay) and [SetInputRegions](../api/SetInputRegions)
