---
title: Debugging your view
sidebar_label: Debugging your view
sidebar_position: 7
---

# Debugging your view

When a PrismaUI view misbehaves, the answer is in one of three places: the framework log, your plugin log, or the in-game inspector. This page is the order to check them in.

## 1. Start with the logs

Two files matter:

- `Documents\My Games\Fallout4\F4SE\PrismaUI_F4.log` — the framework: views created, pages loaded, errors, translation loads, network refusals.
- Your own plugin log — whatever your plugin logs around the Prisma calls.

Open the framework log first. If it says the view was never created, nothing in the page matters yet.

Common lines and what they mean:

| What you see | Likely cause |
| --- | --- |
| `CreateView` failed / invalid view path | Folder name in `CreateView` does not match `views\<name>` |
| no entries at all for your plugin | You never got the API, or you never called `CreateView` |
| `no usable JSON translations` | Missing or malformed translation file |
| CSP / navigation refused | Page tries to load something remote or leave the local folder |

## 2. Capture JavaScript errors

JavaScript errors in the page do not appear in your log unless you ask for them. Register the console callback first thing in `OnDomReady`:

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

    // ... the rest of your setup
}
```

Now `console.error('bad thing')` in the page shows up in your plugin log, with the line you wrote.

## 3. Open the inspector

Press **F12** while a PrismaUI view has focus, when the packaged inspector is available. It is an Ultralight DevTools window inside the game:

- **Elements** — is the element there? is it `display: none`? is the CSS selector right?
- **Console** — the actual exception, with file and line.
- **Network** — did a local asset 404?
- **Sources** — set breakpoints (works in the packaged build).

The inspector is the fastest way to answer "is this a page problem or a bridge problem".

## 4. Decide which half is broken

| Symptom | Page problem | Bridge problem |
| --- | --- | --- |
| Nothing drawn at all | — | view never created, or never shown |
| White/blank box | CSS background covers everything, or a broken script | — |
| Panel drawn, no clicks | page has no focusable elements | view not focused |
| Button does nothing | click handler missing/typo | `BindUIEvent` name mismatch |
| Data does not appear | `window.onSomething` missing | `InteropCall` sent before DOM ready, or wrong name |

For a fast split: call `Invoke(view, "console.log('bridge alive')")` and check the log. If you see it, the bridge works and the problem is in the page.

## 5. Check view health

```cpp
switch (g_api->GetViewHealth(g_view)) {
case PRISMA_UI_API::ViewHealth::kLoadFailed:      REX::WARN("page failed to load"); break;
case PRISMA_UI_API::ViewHealth::kDomReadyTimeout: REX::WARN("page never became ready"); break;
case PRISMA_UI_API::ViewHealth::kUnresponsive:    REX::WARN("page is unresponsive"); break;
case PRISMA_UI_API::ViewHealth::kJsError:         REX::WARN("page has a JS error"); break;
default: break;
}
```

An unhealthy view should be destroyed and recreated, not poked:

```cpp
g_api->Destroy(g_view);
g_view = 0;
EnsureView();
```

## 6. The usual suspects

- **Wrong view path.** `CreateView("MyPlugin/index.html")` needs the folder `Data/PrismaUI_F4/views/MyPlugin/`. Check capital letters.
- **Created too early.** `kGameDataReady` is for the API; views belong at `kPostLoadGame` / `kNewGame`. See [When your code runs](when-your-code-runs).
- **Shown but not focused.** The page is visible but the game still owns input. Call `Focus`.
- **Function name mismatch.** `window.requestClose` is not `window.RequestClose`.
- **Sent before ready.** `InteropCall` before `OnDomReady` finds no page.
- **Remote asset.** A `<link href="https://fonts...">` silently fails and may block your page's style.
- **Transparent page with no opaque element.** You drew it; there is just nothing to see.

## 7. When it only breaks in game

The inspector shows the same page you would see in a browser — but the game is a different environment:

- no network except local files;
- no Web Workers, WebSockets or WebRTC;
- lower memory and a slower JS engine than desktop Chrome;
- the page is inside the game's frame, so a heavy page shows up as stutter.

If it works in the inspector but not in play, look at timers and per-frame work first.

## Where to go next

- [Troubleshooting](troubleshooting) — symptom-driven list
- [Limitations](limitations) — what the runtime does not support
- [The HTML side](html-views) — the page contract
- [Packaging your mod](packaging) — ship it without surprises
