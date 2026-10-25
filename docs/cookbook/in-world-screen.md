---
title: Put a view on an in-world screen
sidebar_label: In-world screen
sidebar_position: 10
---

# Put a view on an in-world screen

This puts a PrismaUI page onto a screen in the world — a custom monitor, a terminal, a TV — instead of drawing it flat over the game view. The view renders offscreen into a texture, and Papyrus binds that texture to an object reference.

This is an advanced surface. Test it in the exact game version and mesh setup you ship.

## How it works

```
your HTML page ──► offscreen view ──► Prisma texture ──► bound to a mesh's material
```

You need three things:

1. a page (`index.html`), like any other Prisma view;
2. `SetOffscreen` + `SetOffscreenSize` so the view renders into a texture at the right resolution;
3. `BindToObject` (by node name) or `BindToTexture` (by material substring) to put it on the mesh.

## 1. The page

Keep it simple and readable at the texture size you choose:

```html
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>Screen</title>
  <style>
    html, body { margin: 0; width: 100%; height: 100%; background: #0a0f0c; color: #9fe0a8; font-family: "Courier New", monospace; }
    main { padding: 24px; }
    h1 { margin: 0 0 12px; font-size: 28px; letter-spacing: 3px; }
    p { margin: 0; font-size: 18px; }
  </style>
</head>
<body>
  <main>
    <h1 id="title">SETTLEMENT NETWORK</h1>
    <p id="status">Online</p>
  </main>
  <script>
    window.setStatus = function (text) {
      document.getElementById('status').textContent = text;
    };
  </script>
</body>
</html>
```

## 2. Papyrus: create the offscreen view and bind it

Attach this to a quest that has a reference to the object you want to use:

```papyrus
Scriptname MyWorldScreen extends Quest

ObjectReference Property ScreenRef Auto

Event OnInit()
    PrismaUI.CreateView("MyWorldScreen", "MyWorldScreen/index.html")

    ; Match the quad's aspect ratio. 1024x512 = 2:1 here.
    PrismaUI.SetOffscreenSize("MyWorldScreen", 1024, 512)
    PrismaUI.SetOffscreen("MyWorldScreen", true)
EndEvent

Event OnCellLoad()
    if ScreenRef
        ; Bind by exact BSGeometry node name, e.g. "Screen:0".
        if !PrismaUI.BindToObject("MyWorldScreen", ScreenRef, "Screen:0", false)
            Debug.Trace("screen bind failed")
        endif
    endif
EndEvent

Event OnCellDetach()
    PrismaUI.Unbind("MyWorldScreen")
EndEvent

Event OnQuestShutdown()
    PrismaUI.Unbind("MyWorldScreen")
    PrismaUI.Destroy("MyWorldScreen")
EndEvent
```

## 3. Binding by material instead of node name

Replacers and mods rename nodes. If you know a recognizable part of the material path, bind by texture substring instead:

```papyrus
PrismaUI.BindToTexture("MyWorldScreen", ScreenRef, "terminalscreen", false)
```

That matches case-insensitively against the current diffuse texture path, so a replacer that keeps the material name still works.

Use `false` for the last argument in normal third-person/world cases; pass `true` when the geometry belongs to the first-person view.

## 4. Send data to the screen

From Papyrus:

```papyrus
PrismaUI.Push("MyWorldScreen", "setStatus", "\"Raiders inbound\"")
```

The page function receives the JSON/string payload; in the example above it just shows the text.

## 5. Cleaning up

- `Unbind` when the object goes away or the player leaves the cell.
- `Destroy` when the feature is disabled.
- Clear offscreen mode if the view returns to a flat overlay:

```papyrus
PrismaUI.SetOffscreen("MyWorldScreen", false)
```

## Notes and limits

- **Match the aspect ratio.** A 2:1 quad with a 1:1 texture looks stretched. Set `SetOffscreenSize` before enabling offscreen mode so the first frame is already correct.
- **The framework owns the texture.** Do not try to keep or release it from Papyrus.
- **Geometry rebuilds unbind you.** If the game rebuilds the object, bind again — hence the `OnCellLoad` bind in the example.
- **Unsupported runtime layouts fail closed.** The feature only binds on verified game versions and mesh-binding support; failures are logged, not guessed. Check the log if nothing appears.
- **Binding is off by default.** It is an opt-in advanced path, not required for normal overlays.
- **Keep the page cheap.** An in-world screen is often always visible while the cell is loaded; animating it every frame costs real performance.

## Where to go next

- [Model preview](../model-preview) — a different feature: 3D *objects* over a page
- [Limitations](../limitations) — mesh binding caveats
- [Open a panel from Papyrus](from-papyrus) — the non-offscreen Papyrus basics
- [Debugging your view](../debugging) — when the screen stays black
