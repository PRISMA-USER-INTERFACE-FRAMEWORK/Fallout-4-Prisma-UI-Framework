# Getting Started with PrismaUI_F4

This guide takes an F4SE C++ plugin from zero to a working PrismaUI panel using the current desktop API surface through V12.

PrismaUI uses Ultralight 1.4.0 in-process. Desktop support covers Fallout 4 `1.10.163` (OG) and `1.11.137+` (AE) with matching Address Library data. The intermediate `1.10.980-1.10.984` line is not supported.

## 1. Install the requirements

You need Visual Studio 2022 with C++, Git, your normal F4SE/CommonLibF4 setup, and PrismaUI installed as a mod.

The runtime contains `PrismaUI_F4.dll` plus the Ultralight runtime under `Data/PrismaUI_F4/`.

## 2. Copy the API header

The desktop SDK now ships as one public header:

```text
src/PrismaUI_F4_API.h   V1-V12
```

Copy that file into your plugin and include it directly:

```cpp
#include "PrismaUI_F4_API.h"
```

`PrismaUI_F4_API.h` contains `IVPrismaUI1` through `IVPrismaUI12`. There are no separate V11 or V12 consumer headers. Request the lowest interface version that provides the features your mod needs.

## 3. Request the interface

Request PrismaUI after `kGameDataReady` and null-check it.

```cpp
static PRISMA_UI_API::IVPrismaUI12* g_api = nullptr;

static void OnMessage(F4SE::MessagingInterface::Message* msg)
{
    if (!msg) return;
    if (msg->type != F4SE::MessagingInterface::kGameDataReady) return;

    g_api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI12>();
    if (!g_api) {
        logger::warn("PrismaUI V12 unavailable");
    }
}
```

Request the lowest interface containing the features your mod requires. Use V10 for panel/input-region features only, V11 when you need verified deferred engine work, and V12 when you need framework-owned controller actions.

## 4. Create a view

Place web assets under your own view folder:

```text
Data/PrismaUI_F4/views/MyPlugin/
  index.html
  app.css
  app.js
```

```cpp
static PrismaView g_view = 0;

static void OnDomReady(PrismaView view)
{
    logger::info("Prisma view {} DOM-ready", view);
}

void EnsureView()
{
    if (!g_api || (g_view && g_api->IsValid(g_view))) return;

    g_view = g_api->CreateView("MyPlugin/index.html", OnDomReady);
    if (g_view) {
        g_api->SetViewRole(g_view, PRISMA_UI_API::ViewRole::kPanel);
        g_api->Hide(g_view);
    }
}
```

`CreateView` begins visible, so normal panels usually hide immediately.

## 5. Bind page events from DOM-ready

DOM-ready is the normal point for page-facing bindings and V12 controller mappings.

```cpp
static void OnDomReady(PrismaView view)
{
    g_api->RegisterJSListener(view, "closePanel", [](const char*) {
        if (!g_api || !g_api->IsValid(g_view)) return;
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
    });

    g_api->BindControllerAction(view, "X", "panel.secondary");
    g_api->BindControllerAction(view, "Y", "panel.favorite");
    g_api->BindControllerAction(view, "LB", "panel.previous_tab");
    g_api->BindControllerAction(view, "RB", "panel.next_tab");
}
```

`BindControllerAction` returns false before the final document is DOM-ready.

## 6. Receive controller actions in JavaScript

```js
window.addEventListener('prisma-controller-action', ({ detail }) => {
  if (detail.state !== 'pressed') return;

  switch (detail.action) {
    case 'panel.secondary':
      openSecondaryAction();
      break;
    case 'panel.favorite':
      toggleFavorite();
      break;
  }
});
```

The event includes `detail.action`, `detail.button`, and `detail.state` (`pressed`, `repeat`, `released`).

Canonical V12 buttons are:

`A`, `B`, `X`, `Y`, `LB`, `RB`, `LT`, `RT`, `LS`, `RS`, `Back`, `Start`, `DUp`, `DDown`, `DLeft`, `DRight`.

Unmapped D-pad/A/B continue to produce Arrow/Enter/Escape navigation automatically. Explicitly mapping one replaces its legacy synthetic-key route without duplicate delivery.

See [Controller Actions](controller-actions) for the complete contract, trigger hysteresis, focus ownership, lifecycle, prompts, and migration from raw controller sinks.

## 7. Mutate Fallout state through V11

Controller JavaScript delivery is UI-thread work. If page code must mutate Fallout-owned state, bind a verified V11 game-thread event:

```cpp
static void OnApply(const char* argument, void*)
{
    // Fallout-owned work here.
}

static void OnDomReady(PrismaView view)
{
    g_api->BindGameThreadUIEvent(view, "applySetting", OnApply, nullptr);
}
```

Or queue native work explicitly:

```cpp
g_api->DispatchToGameThread([](void*) {
    // Deferred verified Fallout window-thread work.
}, nullptr);
```

A rejected `DispatchToGameThread` call returns false and will not execute later.

## 8. Show and focus the panel

```cpp
void OpenPanel()
{
    if (!g_api || !g_api->IsValid(g_view)) return;
    g_api->Show(g_view);
    g_api->Focus(g_view, false, false);
}

void ClosePanel()
{
    if (!g_api || !g_api->IsValid(g_view)) return;
    g_api->Unfocus(g_view);
    g_api->Hide(g_view);
}
```

For selective overlay input, check the `InputRegions` capability before using `SetInputRegions` and `FocusOverlay`.

## 9. Send data between C++ and JavaScript

C++ to JavaScript:

```cpp
g_api->InteropCall(g_view, "setStatus", R"({"ready":true})");
```

JavaScript can call functions registered by `RegisterJSListener`, `BindUIEvent`, or V11 `BindGameThreadUIEvent`, depending on the required thread/engine semantics.

## 10. Controller prompts and glyphs

V12 owns focused controller actions. V9 prompt APIs remain the presentation layer:

- `IsUsingGamepad`
- `GetControllerStyle`
- `GetButtonPrompt`
- `GetGamepadButtonName`

Use PrismaUI's shared controller glyph system rather than shipping a second copy of Xbox/PlayStation button art.

## 11. Lifecycle cleanup

V12 mappings are per view. You can remove one with `UnbindControllerAction`, clear all with `ClearControllerActions`, or rely on `Destroy(view)` to clear the view's mappings before backend destruction.

## 12. Bundle web dependencies locally

Do not make a shipped view depend on CDNs, Google Fonts, or previous-browser-runtime-only behavior. Bundle required HTML, JS, CSS, fonts and images with your mod. Implement real HTTP/WebSocket/authentication work in native C++ and relay validated data to the page.

## 13. Debugging

Use:

- `PrismaUI_F4.log`
- your plugin log
- `RegisterConsoleCallback`
- `GetViewHealth`
- external front-end testing for normal HTML/CSS/JS behavior

## Before you ship

Verify that your plugin:

- includes the canonical `PrismaUI_F4_API.h` SDK header;
- null-checks `RequestPluginAPI`;
- requests no newer interface than it needs;
- binds V12 actions only after DOM-ready;
- routes engine mutations through appropriate V11/native ownership;
- handles focus/unfocus deliberately;
- does not duplicate V12 controller actions with a second raw focused-view input sink;
- keeps required assets local;
- tests each Fallout runtime family it claims to support.

Next: [API Reference](api-reference), [Controller Actions](controller-actions), [API Extensions](api-extensions), [View Lifecycle](view-lifecycle), [Networking](networking), and [Troubleshooting](troubleshooting).
