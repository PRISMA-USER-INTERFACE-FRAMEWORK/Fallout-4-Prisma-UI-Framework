---
id: model-preview
title: Model preview (3D rendering)
sidebar_label: Model preview
sidebar_position: 11
---

# Model preview (3D rendering)

ModelPreview draws a real game mesh over your page — a weapon spinning in an inventory screen, a piece of armor next to its stats, a settlement object in a build menu. The game renders it; your page only says where it goes.

The released JavaScript bridge is **ModelPreview v4** and lives at `window.__prismaUI_modelPreview`.

If you just want a working example, start with [Show a 3D item preview](cookbook/item-preview) and come back here for the full field list.

## Is it there?

PrismaUI installs the bridge just before your `OnDomReady` callback runs, when the feature is enabled:

```js
const preview = window.__prismaUI_modelPreview;

if (!preview || preview.version < 4) {
  // No preview support here. Show your static fallback.
}
```

Never assume the bridge exists. A page with a static image fallback is a page that still works on a build without previews.

## Show a preview

The bridge takes **NIF paths**, not form lookups:

```js
window.__prismaUI_modelPreview.show({
  id: 'selected-item',
  nifPath: 'Meshes/Weapons/10mmPistol/10mmPistol.nif',
  formType: 'WEAP',
  x: 620, y: 120, w: 300, h: 300,
  zoom: 1.0,
  pitch: 0,
  roll: 0,
  yaw: 0.4,
  brightness: 1.0,
});
```

| Field | Meaning |
| --- | --- |
| `id` | Stable name for this preview inside the view. Show/hide by id. |
| `nifPath` | The mesh to load. |
| `nifPaths` | Array of meshes for a composed model; used instead of `nifPath`. |
| `x`, `y`, `w`, `h` | Destination rectangle in view pixels. |
| `formType` | Posing hint (see below). |
| `zoom`, `panX`, `panY`, `pitch`, `roll`, `yaw`, `spin`, `brightness`, `flip` | Camera and appearance adjustments. |
| `clipX`, `clipY`, `clipW`, `clipH` | Optional smaller visible area inside the destination rectangle. |
| `matSwap` | Material swap to apply. |

A request with neither `nifPath` nor a non-empty `nifPaths` is ignored.

## `formType`: how the mesh is posed

| Value | Behaviour |
| --- | --- |
| `WEAP` | Weapon presentation |
| `AMMO` | Ammo presentation |
| `ARMO` | Armor presentation |
| `STAT`, `FURN`, `WORLD` | Keep the authored world orientation |
| other or empty | Generic presentation |

Use `STAT`/`FURN`/`WORLD` for settlement and world objects; they were added in v4 so furniture does not get inventory-style posing.

## Several meshes in one preview

```js
preview.show({
  id: 'assembled',
  nifPaths: ['Meshes/MyMod/base.nif', 'Meshes/MyMod/attachment.nif'],
  x: 620, y: 120, w: 300, h: 300,
});
```

## Clipping

When the model can be larger than the visible box:

```js
preview.show({
  id: 'inv',
  nifPath: 'Meshes/MyMod/item.nif',
  x: 600, y: 100, w: 360, h: 360,
  clipX: 620, clipY: 120, clipW: 320, clipH: 300,
});
```

## Hide it again

```js
preview.hide({ id: 'selected-item' });   // one preview
preview.hide({});                        // everything in this view
```

Call `hide()` when the panel closes or the selection no longer needs a model. A hidden Prisma view is still alive, so previews you leave behind keep native resources around.

## Listen for load status

Register status handling first — it is how you find out that a modded mesh failed:

```js
if (preview && preview.onStatus) {
  preview.onStatus(function (status) {
    if (status.id !== 'selected-item') return;

    if (!status.ok) {
      showFallback(status.id, status.error);
    }
  });
}
```

Status payloads include `id`, `nifPath`, `phase`, `ok`, `error`, and shape diagnostics. Phases you will see:

| Phase | Meaning |
| --- | --- |
| `loaded` | The mesh loaded and is drawable. |
| `textured` | The preview reached its textured, render-ready state. |
| `error` | The mesh was missing, unreadable, or had no drawable shapes. |

Do not build on the old `window.onModelPreviewStatus` / `status === "loaded"` contract. It is not the released API.

## Putting it on the page

```html
<div id="preview-region"></div>
<p id="preview-fallback" hidden>No preview available</p>
```

```js
const preview = window.__prismaUI_modelPreview;
const region = document.getElementById('preview-region');
const fallback = document.getElementById('preview-fallback');

function showNif(nifPath, formType) {
  if (!preview || preview.version < 4) {
    fallback.hidden = false;
    return;
  }

  const r = region.getBoundingClientRect();
  preview.show({
    id: 'selection',
    nifPath,
    formType,
    x: Math.round(r.left),
    y: Math.round(r.top),
    w: Math.round(r.width),
    h: Math.round(r.height),
    zoom: 1.0,
  });
}

if (preview && preview.onStatus) {
  preview.onStatus(function (s) {
    if (s.id !== 'selection') return;
    fallback.hidden = !!s.ok;
  });
}
```

Coordinates are view-space pixels. `getBoundingClientRect()` gives you the right numbers at the default view scale; re-read it after layout changes.

## How the rendering works (and why it costs something)

This is not an `<img>` or a `<canvas>`. PrismaUI loads and parses the NIF, creates native D3D11 resources, and the compositor draws that texture over your view rectangle.

Practical consequences:

- Engine file reads happen through the game thread, budgeted per pass — so a catalog of hundreds of previews does not read the archives all at once.
- Preview parsing and GPU work can run on a worker pool.
- There is a ceiling on resident previews; hidden views are the first to lose theirs.

For your page, that means:

1. show only what the player is looking at;
2. use stable ids;
3. `hide()` when done;
4. always keep a fallback;
5. test with mod-added meshes in the actual load order.

## When a preview fails

`ok: false` plus `error` is the supported signal. Show your fallback image or an empty slot — do not leave a blank rectangle where the model should be. The framework also logs preview details to `PrismaUI_F4.log`.

## C++ side

Nothing extra to call. Create the view normally; the bridge is installed by PrismaUI's DOM-ready setup when the feature is enabled:

```cpp
static void OnDomReady(PrismaView view)
{
    g_api->Invoke(view, "window.init && window.init()");
}

g_view = g_api->CreateView("MyPlugin/inventory.html", OnDomReady);
```

## The old contract

Older PrismaUI builds used a different preview bridge. Those examples are documented in old material only (source: earlier PrismaUI preview documentation). Do not copy them:

```js
window.prismaModelPreview
window.onModelPreviewStatus
destLeft / destTop / destRight / destBottom
```

They do not match the released bridge.

## Where to go next

- [Show a 3D item preview](cookbook/item-preview) — complete recipe
- [The HTML side](html-views) — page basics
- [Limitations](limitations) — what the runtime does not support
- [Debugging your view](debugging) — when the preview does not show
