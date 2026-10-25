---
title: Show a 3D item preview
sidebar_label: 3D item preview
sidebar_position: 5
---

# Show a 3D item preview

This puts a real game mesh inside your page: the player clicks a row, and the item is rendered over a region of the page by the game renderer. It is the same idea as the item previews in the vanilla inventory, but placed wherever you want.

The JavaScript API is **ModelPreview v4**. It is installed by PrismaUI before your `OnDomReady`, so the plugin itself does not need any extra call.

## 1. The page

```html
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>Preview</title>
  <link rel="stylesheet" href="preview.css">
</head>
<body>
  <main class="wrap">
    <div class="list">
      <button class="item" data-nif="Meshes/Weapons/10mmPistol/10mmPistol.nif" data-type="WEAP">10mm Pistol</button>
      <button class="item" data-nif="Meshes/Weapons/CombatRifle/CombatRifle.nif" data-type="WEAP">Combat Rifle</button>
      <button class="item" data-nif="Meshes/Armor/PowerArmor/Frame/PAFrame.nif" data-type="ARMO">Power Armor Frame</button>
    </div>
    <div class="stage">
      <div id="preview-region"></div>
      <p id="preview-fallback" hidden>Preview unavailable</p>
    </div>
  </main>
  <script src="preview.js"></script>
</body>
</html>
```

```css
html, body {
  margin: 0;
  width: 100%;
  height: 100%;
  background: rgba(6, 8, 7, 0.95);
  color: #d4d4d4;
  font-family: "Courier New", monospace;
}

.wrap { display: flex; gap: 24px; padding: 40px; height: 100%; box-sizing: border-box; }
.list { display: flex; flex-direction: column; gap: 8px; width: 220px; }
.item { padding: 10px; background: #14201a; color: inherit; border: 1px solid #3d5a45; font: inherit; cursor: pointer; }
.item:hover, .item.selected { background: #27402f; }
.stage { flex: 1; position: relative; border: 1px solid #3d5a45; }
#preview-region { width: 100%; height: 100%; }
#preview-fallback { position: absolute; inset: 0; display: grid; place-items: center; margin: 0; color: #888; }
```

```js
const preview = window.__prismaUI_modelPreview;
const stage = document.getElementById('preview-region');
const fallback = document.getElementById('preview-fallback');

// Tell the user when a preview cannot be shown.
if (preview && preview.onStatus) {
  preview.onStatus(function (status) {
    if (status.id !== 'selected') return;
    fallback.hidden = !!status.ok;
    if (!status.ok) console.warn('preview failed: ' + status.error);
  });
}

function show(nifPath, formType) {
  if (!preview || preview.version < 4) {
    fallback.hidden = false;
    return;
  }

  // The preview rectangle is in view pixels: use where the stage sits on screen.
  const r = stage.getBoundingClientRect();

  preview.show({
    id: 'selected',
    nifPath: nifPath,
    formType: formType,
    x: Math.round(r.left),
    y: Math.round(r.top),
    w: Math.round(r.width),
    h: Math.round(r.height),
    zoom: 1.0,
    pitch: 0,
    roll: 0,
    yaw: 0.4,
  });
}

document.querySelectorAll('.item').forEach(function (button) {
  button.addEventListener('click', function () {
    document.querySelectorAll('.item').forEach(function (b) { b.classList.remove('selected'); });
    button.classList.add('selected');
    show(button.dataset.nif, button.dataset.type);
  });
});

// Hide native previews when the page goes away or the panel is hidden.
window.addEventListener('unload', function () {
  if (preview) preview.hide({});
});
```

## 2. Plugin side

No special call is needed. Create the view normally and show it. The preview bridge appears on the page as `window.__prismaUI_modelPreview` when the feature is available.

If you want to hide previews when the panel closes:

```cpp
g_api->BindUIEvent(view, "closeRequested", [](const char*) {
    g_api->Invoke(g_view, "window.__prismaUI_modelPreview && window.__prismaUI_modelPreview.hide({});");
    g_api->Unfocus(g_view);
    g_api->Hide(g_view);
});
```

That `Invoke` is belt and braces: hiding the view also stops preview work, but explicit `hide()` releases the native resources sooner.

## Notes

- **Check the bridge.** `window.__prismaUI_modelPreview` is only defined when ModelPreview is enabled in the running build. Always keep the fallback path.
- **Coordinates are view pixels.** `getBoundingClientRect()` on the stage div gives the right numbers at the default view scale.
- **Use stable IDs.** `id: 'selected'` here, one per slot. Showing a second preview with the same id replaces the first.
- **`formType` changes the pose.** `WEAP`, `AMMO`, `ARMO` use the inventory-style presentation; `STAT`, `FURN` and `WORLD` keep the authored orientation, which is what you want for settlement and world meshes.
- **Modded meshes can fail.** The status callback gives you `ok: false` and an `error` string. Show your fallback instead of an empty box.
- **Hide what you do not show.** Every visible preview costs render work. Call `hide()` when the selection changes to a non-preview row or the panel closes.
- **Mixing several meshes** is possible with `nifPaths: [...]` instead of `nifPath` when an item is assembled from parts.

## Where to go next

- [Model Preview](../model-preview) for every v4 field and behavior
- [Talk between the page and the plugin](talk-to-your-plugin) to pass the selected item from C++
- [Limitations](../limitations) for what can and cannot be rendered
