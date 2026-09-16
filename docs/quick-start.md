---
id: quick-start
title: Quick Start
sidebar_label: Quick Start
sidebar_position: 2
---

# Quick Start

This is the shortest path from an existing F4SE plugin to a current PrismaUI panel using the flat V12 API.

## 1. Copy/include the API header

```cpp
#include "PrismaUI_F4_API.h"
```

`PrismaUI_F4_API.h` is the complete desktop SDK header and contains V1 through V12.

## 2. Request V12

```cpp
static PRISMA_UI_API::IVPrismaUI12* g_api = nullptr;
static PrismaView g_view = 0;

// During/after kGameDataReady:
g_api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI12>();
if (!g_api) {
    logger::warn("PrismaUI V12 unavailable");
    return;
}
```

If your mod does not use V11/V12 features, request the lowest older interface it actually needs.

## 3. Create the view

```cpp
static void OnDomReady(PrismaView view)
{
    g_api->BindControllerAction(view, "X", "panel.secondary");
    g_api->BindControllerAction(view, "LB", "panel.previous");
    g_api->BindControllerAction(view, "RB", "panel.next");
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

V12 bindings are registered after DOM-ready. Binding before the final document is ready returns false.

## 4. Handle controller actions

```js
window.addEventListener('prisma-controller-action', ({ detail }) => {
  if (detail.state !== 'pressed') return;

  if (detail.action === 'panel.secondary') openSecondaryAction();
  if (detail.action === 'panel.previous') previousTab();
  if (detail.action === 'panel.next') nextTab();
});
```

The framework routes mapped controller events only to the exact focused live view. Unmapped D-pad/A/B keep the normal Arrow/Enter/Escape route.

## 5. Show and focus

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

## 6. Engine mutations use V11

If JavaScript needs to mutate Fallout state, expose a V11 `BindGameThreadUIEvent` callback or use `DispatchToGameThread`. Do not treat the Ultralight JavaScript context as an engine thread.

## 7. Deploy

```text
Data/F4SE/Plugins/MyPlugin.dll
Data/PrismaUI_F4/views/MyPlugin/index.html
```

Bundle all required web assets locally.

## Controller button names

`A`, `B`, `X`, `Y`, `LB`, `RB`, `LT`, `RT`, `LS`, `RS`, `Back`, `Start`, `DUp`, `DDown`, `DLeft`, `DRight`.

LT/RT use 0.55 press and 0.45 release hysteresis. Mapping A/B/D-pad explicitly replaces that button's legacy Enter/Escape/Arrow delivery rather than duplicating it.

## Next

- [Getting Started](getting-started)
- [Controller Actions](controller-actions)
- [API Reference V1-V12](api-reference)
- [Current API Extensions](api-extensions)
- [View Lifecycle](view-lifecycle)
- [Troubleshooting](troubleshooting)
