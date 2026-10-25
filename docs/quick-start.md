---
id: quick-start
title: Quick Start
sidebar_label: Quick Start
sidebar_position: 2
---

# Quick Start

This is the shortest path from an existing F4SE plugin to a current PrismaUI panel using the preferred modern feature-table API.

## 1. Include the modern header

```cpp
#include "PrismaUI_F4_Modern_API.h"

using namespace PRISMA_UI_FLAT_API;
```

The modern header exposes independently versioned feature tables. Existing plugins built against `PrismaUI_F4_API.h` V1-V12 remain supported.

## 2. Discover only what you need

```cpp
static ViewAPI g_viewApi{};
static ControllerAPI g_controller{};
static PrismaView g_view = 0;

bool LoadPrisma()
{
    return Discover<ApiFeature::View>(ViewApiVersion, g_viewApi) &&
           Discover<ApiFeature::Controller>(ControllerApiVersion, g_controller);
}
```

Call `LoadPrisma()` during or after `kGameDataReady`. If discovery fails, do not call through that table.

## 3. Create the view

```cpp
static void OnDomReady(PrismaView view)
{
    g_controller.BindControllerAction(view, "X", "panel.secondary");
    g_controller.BindControllerAction(view, "LB", "panel.previous");
    g_controller.BindControllerAction(view, "RB", "panel.next");
}

void EnsureView()
{
    if (g_view && g_viewApi.IsValid(g_view)) return;

    g_view = g_viewApi.CreateView("MyPlugin/index.html", OnDomReady);
    if (g_view) {
        g_controller.SetViewRole(g_view, ViewRole::kPanel);
        g_viewApi.Hide(g_view);
    }
}
```

Controller bindings are registered after DOM-ready. PrismaUI owns bridge installation and bounded recovery.

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
    if (!g_viewApi.IsValid(g_view)) return;
    g_viewApi.Show(g_view);
    g_viewApi.Focus(g_view, false, false);
}

void ClosePanel()
{
    if (!g_viewApi.IsValid(g_view)) return;
    g_viewApi.Unfocus(g_view);
    g_viewApi.Hide(g_view);
}
```

## 6. Add features as needed

Discover additional tables only when the plugin needs them:

- `InteropAPI` for `Invoke`, JS listeners, UI events, and console callbacks;
- `GameThreadAPI` for verified Fallout-thread dispatch and UI event callbacks;
- `LocalizationAPI` for V4 JSON localization;
- `RenderAPI`, `InputAPI`, or `MenuAPI` for their focused contracts.

Do not infer feature support from the framework version when a feature-table query is available.

## 7. Deploy

```text
Data/F4SE/Plugins/MyPlugin.dll
Data/PrismaUI_F4/views/MyPlugin/index.html
```

Bundle all required web assets locally.

## Compatibility API

Existing numbered-interface integrations can continue using `PrismaUI_F4_API.h` V1-V12. See [API Reference V1-V12](api-reference) and [Current API Extensions](api-extensions) when maintaining that ABI.

## Next

- [Modern API](modern-api)
- [Getting Started](getting-started)
- [Controller Actions](controller-actions)
- [Translations](translations)
- [View Lifecycle](view-lifecycle)
- [Troubleshooting](troubleshooting)
