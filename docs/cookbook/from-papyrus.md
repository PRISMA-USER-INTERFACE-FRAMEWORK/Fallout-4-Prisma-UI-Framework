---
title: Open a panel from Papyrus
sidebar_label: From Papyrus
sidebar_position: 8
---

# Open a panel from Papyrus

You do not always need C++. PrismaUI ships a Papyrus script that can create, show, hide and focus a view, push data into the page, and even bind a view to an in-game object. This recipe builds a Papyrus-only panel that opens with **F3**.

Use this for simple interfaces: a notice board, a small menu, a terminal-style page. For controller actions, game-thread work and the full API, you still want C++.

## 1. The page

Same as any other PrismaUI page. Create:

```text
Data/PrismaUI_F4/views/MyPapyrusPanel/index.html
Data/PrismaUI_F4/views/MyPapyrusPanel/app.css
Data/PrismaUI_F4/views/MyPapyrusPanel/app.js
```

```html
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>Notice Board</title>
  <link rel="stylesheet" href="app.css">
</head>
<body>
  <main class="panel">
    <h1 id="title">NOTICE BOARD</h1>
    <p id="body">Nothing new today.</p>
    <p class="hint">Press F3 to close.</p>
  </main>
  <script src="app.js"></script>
</body>
</html>
```

```css
html, body { margin: 0; width: 100%; height: 100%; background: transparent; font-family: "Courier New", monospace; }
.panel { width: 460px; margin: 15vh auto 0; padding: 18px 20px; background: rgba(8, 12, 10, 0.94); border: 1px solid #3d5a45; color: #d4d4d4; }
h1 { margin: 0 0 10px; font-size: 18px; letter-spacing: 2px; }
.hint { margin-top: 16px; color: #8a8a8a; font-size: 12px; }
```

```js
// Papyrus calls this with Push("MyPapyrusPanel", "setNotice", jsonText).
window.setNotice = function (jsonText) {
  try {
    const data = JSON.parse(jsonText);
    document.getElementById('title').textContent = data.title || 'NOTICE BOARD';
    document.getElementById('body').textContent = data.body || 'Nothing new today.';
  } catch (e) {
    console.warn('bad notice payload: ' + jsonText);
  }
};
```

## 2. The Papyrus script

Make a quest script (this example is called `MyNoticePanel`):

```papyrus
Scriptname MyNoticePanel extends Quest

bool panelOpen = false

Event OnInit()
    ; The second argument is the page path under Data/PrismaUI_F4/views/.
    PrismaUI.CreateView("MyPapyrusPanel", "MyPapyrusPanel/index.html")
    PrismaUI.Hide("MyPapyrusPanel")
    RegisterForKey(0x3D)        ; F3
EndEvent

Event OnKeyDown(int keyCode)
    if keyCode != 0x3D
        return
    endif

    ; A save load can leave an old view invalid. Recreate if needed.
    if !PrismaUI.IsValid("MyPapyrusPanel")
        PrismaUI.CreateView("MyPapyrusPanel", "MyPapyrusPanel/index.html")
    endif

    panelOpen = !panelOpen
    if panelOpen
        PrismaUI.Show("MyPapyrusPanel")
        PrismaUI.Focus("MyPapyrusPanel", true, false)   ; pause the game, show the focus menu
    else
        PrismaUI.Unfocus("MyPapyrusPanel")
        PrismaUI.Hide("MyPapyrusPanel")
    endif
EndEvent
```

Attach the script to a quest, make sure the quest is running, and start the game. **F3** toggles the panel.

## 3. Send data from Papyrus into the page

`Push(name, functionName, json)` calls a JavaScript function on the page. The argument is the JSON string:

```papyrus
Event SomeGameEvent()
    PrismaUI.Push("MyPapyrusPanel", "setNotice", "{\"title\":\"SETTLEMENT\",\"body\":\"Raiders seen north of the bridge.\"}")
EndEvent
```

The page function receives the raw string, so parse it in JavaScript (as in `app.js` above). Keep the payload small; this path is for display data, not bulk transfers.

## 4. Bind the view to an in-game object

Papyrus can also put a view onto a screen or a quad in the world:

```papyrus
; By an exact geometry node name, for example the screen of a custom mesh.
PrismaUI.BindToObject("MyPapyrusPanel", myObjectReference, "Screen:0", false)

; Or by a substring of the diffuse texture path, which survives replacer meshes.
PrismaUI.BindToTexture("MyPapyrusPanel", myObjectReference, "terminalscreen", false)
```

The framework retries for a moment while the 3D object and the view texture come online. Unbind when the object should stop showing the view:

```papyrus
PrismaUI.Unbind("MyPapyrusPanel")
```

If the view should be offscreen (a texture bound to geometry) rather than drawn flat on the screen, set that up first:

```papyrus
PrismaUI.SetOffscreenSize("MyPapyrusPanel", 1024, 512)   ; match the quad's aspect ratio
PrismaUI.SetOffscreen("MyPapyrusPanel", true)
```

## 5. What Papyrus cannot do

The bridge is small on purpose:

- it cannot receive page events; the page cannot call back into Papyrus. Send data one way (`Push`) and close from a key or game event.
- it has no controller actions, no input regions, no ModelPreview control, no translations registration.
- it has no game-thread dispatch for arbitrary work.

When you need one of those, move the view control into a small C++ plugin and keep the Papyrus side for the quest logic that decides when to open it.

## Notes

- **The view name is yours to choose.** Use the same string everywhere: `CreateView`, `Show`, `Hide`, `Push`. It is not the folder name, though using the same value keeps things clear.
- **Create once, not every keypress.** `CreateView` on every toggle leaks views.
- **Hide on load.** A view starts visible. Always `Hide` right after creating it if it is a menu.
- **Focus pauses the game.** `Focus(name, true, false)` = pause while open, normal focus menu behaviour.
- **F3 is `0x3D`.** Papyrus uses DirectX scan codes. `0x3D` is F3; `0x3C` is F2, `0x3E` is F4.

## Where to go next

- [Papyrus bridge](../papyrus-bridge) — the full reference for the script
- [Your first panel](../quick-start) — the same panel built from C++
- [Packaging your mod](../packaging) — shipping the page and script
