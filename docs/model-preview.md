---
id: model-preview
title: Model Preview (3D Rendering)
sidebar_label: Model Preview
sidebar_position: 11
---

# Model Preview (3D Rendering)

PrismaUI_F4 2.1.0 can composite native D3D11 model previews over a Prisma view. The released JavaScript bridge is **ModelPreview API v4**.

This page documents the actual 2.1.0 bridge. Older examples that use `window.prismaModelPreview`, `formId`, `pluginName`, or `destLeft`/`destRight` belong to an earlier design and do not match the released API.

## Availability

When ModelPreview is enabled, PrismaUI installs the bridge immediately before the consumer view's DOM-ready callback:

```js
window.__prismaUI_modelPreview
```

The object exposes:

```js
{
  version: 4,
  show(options),
  hide(options),
  onStatus(callback)
}
```

Check for the bridge instead of assuming it exists:

```js
const preview = window.__prismaUI_modelPreview;
if (!preview || preview.version < 4) {
  // Show your normal static fallback.
}
```

## Register status handling first

```js
const preview = window.__prismaUI_modelPreview;

preview.onStatus((status) => {
  console.log('ModelPreview', status);

  if (!status.ok) {
    showFallback(status.id, status.error);
  }
});
```

Status payloads include at least:

- `id`
- `nifPath`
- `phase`
- `ok`
- `error`
- shape diagnostics when a model is available

Observed release phases include:

- `loaded` - a drawable model is ready;
- `textured` - the loaded preview reached its textured/render-ready state;
- `error` - the mesh was missing, unreadable, or contained no drawable shapes.

Do not build status handling around the old `window.onModelPreviewStatus` or `status: "loaded"` contract.

## Show a preview

The release bridge takes **NIF paths**, not a FormID/plugin lookup.

```js
window.__prismaUI_modelPreview.show({
  id: 'selected-item',
  nifPath: 'Meshes/Weapons/10mmPistol/10mmPistol.nif',
  formType: 'WEAP',
  x: 620,
  y: 120,
  w: 300,
  h: 300,
  zoom: 1.0,
  panX: 0,
  panY: 0,
  pitch: 0,
  roll: 0,
  brightness: 1.0
});
```

### Required/practical fields

| Field | Meaning |
|---|---|
| `id` | Stable identifier for this preview within the view. Use one when you may show/hide previews independently. |
| `nifPath` | One NIF path to load. |
| `nifPaths` | Alternative array of NIF paths for a composed preview. If present, it is used instead of `nifPath`. |
| `x`, `y` | Destination rectangle origin in Prisma view pixels. |
| `w`, `h` | Destination rectangle size in Prisma view pixels. |

A request with neither `nifPath` nor a non-empty `nifPaths` array is ignored.

## Optional request fields

The released request parser also accepts:

- `formType`
- `matSwap`
- `clipX`, `clipY`, `clipW`, `clipH`
- `zoom`
- `panX`, `panY`
- `roll`
- `pitch`
- `brightness`
- `spin`
- `yaw`
- `flip`

Use only what your UI needs and test the exact mesh/material combination in game.

## `formType`

`formType` is a presentation hint used by the native pose logic.

Known 2.1.0 handling includes:

| `formType` | Native pose behavior |
|---|---|
| `WEAP` | Elongated/weapon presentation |
| `AMMO` | Ammo presentation |
| `ARMO` | Armor presentation |
| `STAT` | Preserve authored world orientation |
| `FURN` | Preserve authored world orientation |
| `WORLD` | Preserve authored world orientation |
| other/empty | Generic presentation |

`STAT`, `FURN`, and `WORLD` were added to the v4 contract specifically so settlement/furniture/static meshes remain upright instead of receiving inventory-style auto posing.

## Multiple NIFs

For a preview composed from several meshes:

```js
preview.show({
  id: 'assembled-item',
  nifPaths: [
    'Meshes/MyMod/base.nif',
    'Meshes/MyMod/attachment.nif'
  ],
  x: 620,
  y: 120,
  w: 300,
  h: 300
});
```

The status payload's `nifPath` field is a display representation of the request's NIF path set.

## Clipping

Use the clip fields when the model should remain inside a smaller visible region than its destination rectangle:

```js
preview.show({
  id: 'inventory-preview',
  nifPath: 'Meshes/MyMod/item.nif',
  x: 600,
  y: 100,
  w: 360,
  h: 360,
  clipX: 620,
  clipY: 120,
  clipW: 320,
  clipH: 300
});
```

Coordinates are view-space pixels.

## Hide previews

Hide one preview by ID:

```js
preview.hide({ id: 'selected-item' });
```

Hide all ModelPreview instances belonging to the current Prisma view by omitting the ID:

```js
preview.hide({});
```

Call `hide()` when a panel closes or stops needing the preview. A Prisma view can remain alive while hidden, so explicitly releasing preview work avoids unnecessary native rendering/resource residency.

## Suggested DOM integration

```html
<div id="preview-region"></div>
<div id="preview-fallback" hidden>No preview available</div>
```

```js
const preview = window.__prismaUI_modelPreview;
const region = document.getElementById('preview-region');
const fallback = document.getElementById('preview-fallback');

function showNif(nifPath, formType) {
  if (!preview) {
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
    zoom: 1.0
  });
}

if (preview) {
  preview.onStatus((s) => {
    if (s.id !== 'selection') return;
    fallback.hidden = !!s.ok;
  });
}
```

## Native behavior and performance

ModelPreview is not an Ultralight `<canvas>` or `<img>` feature. PrismaUI loads/parses the NIF and creates native D3D11 render resources, then the compositor places that texture over the requested view rectangle.

Important 2.1.0 behavior:

- Model parsing/GPU work can run on a worker pool.
- Bethesda/engine-backed file reads are marshalled to the engine/render thread.
- Engine file reads are budgeted per servicing pass so a large catalog does not drain an unbounded number of synchronous archive reads in one frame.
- Resident previews have a configured ceiling to bound resource growth.
- Hidden/inactive views are cheaper eviction candidates.

For consumer code, the practical rules are simpler:

1. show only the preview(s) the player currently needs;
2. use stable IDs;
3. call `hide()` when the panel closes or selection no longer needs the model;
4. keep a visual fallback for load errors;
5. test mod-added meshes/materials in the actual load order.

## Material and mesh failures

The status callback is the supported way to detect preview failure. Do not assume every mod-added NIF/material can be rendered.

When `ok` is false, display a fallback image/panel and retain the `error` text for diagnostics. The framework also logs ModelPreview details to `PrismaUI_F4.log`.

## C++ side

There is no extra public C++ interface call needed to create the JavaScript bridge. Create the Prisma view normally:

```cpp
static void OnDomReady(PrismaView view)
{
    g_api->Invoke(view, "window.init && window.init()");
}

g_view = g_api->CreateView("MyPlugin/inventory.html", OnDomReady);
```

The ModelPreview listeners/API object are installed by PrismaUI's DOM-ready setup before your callback is dispatched, when the feature is enabled.

## Do not use the old contract

For PrismaUI_F4 2.1.0, do not copy examples based on:

```js
window.prismaModelPreview
window.onModelPreviewStatus
{ formId, pluginName, destLeft, destTop, destRight, destBottom }
```

Those names/fields do not match the released bridge implementation.
