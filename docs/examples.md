# Examples

The full, working examples live in two places now:

- **[Cookbook](/docs/cookbook/)** — complete recipes with page and plugin code: messaging, HUD overlays, controller menus, item previews, game-thread work, Papyrus, translations, settings and in-world screens. Start here.
- **[example-f4se-plugin](https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Fallout-4-Prisma-UI-Framework/tree/main/example-f4se-plugin)** — the small plugin used by the guides, including the key handler and clipboard code.

This page keeps the canonical minimal panel and a short directory of what to look at for each task.

## The minimal panel

Plugin:

```cpp
#include "PrismaUI_F4_API.h"

static PRISMA_UI_API::IVPrismaUI10* g_api  = nullptr;
static PrismaView                   g_view = 0;

static void OnDomReady(PrismaView view)
{
    g_api->RegisterJSListener(view, "requestClose", [](const char*) {
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
    });
}

static void CreatePanel()
{
    if (!g_api || (g_view && g_api->IsValid(g_view))) return;

    g_view = g_api->CreateView("MyPlugin/panel.html", OnDomReady);
    if (!g_view) return;

    g_api->SetViewRole(g_view, PRISMA_UI_API::ViewRole::kPanel);
    g_api->Hide(g_view);
}

static void OpenPanel()
{
    if (!g_api || !g_api->IsValid(g_view)) return;
    if (g_api->IsAnyPanelVisible(g_view)) return;

    g_api->Show(g_view);
    g_api->Focus(g_view, /*pauseGame=*/true);
}
```

Page:

```html
<button id="close" type="button">Close</button>
<script>
  document.querySelector('#close').addEventListener('click', () => window.requestClose());
</script>
```

The walkthrough that builds this from an empty project is [Your first panel](quick-start).

## Find the example for your task

| I want to… | Look at |
| --- | --- |
| Build my first panel | [Your first panel](quick-start) |
| Send JSON from C++ to the page and back | [Talk between the page and the plugin](cookbook/talk-to-your-plugin) |
| Add a HUD that does not block the game | [A click-through HUD overlay](cookbook/hud-overlay) |
| Make a menu that works with a gamepad | [A controller-friendly menu](cookbook/controller-menu) |
| Draw a real 3D item in my page | [Show a 3D item preview](cookbook/item-preview) |
| Change game state from a page button | [Run game-thread work from a button](cookbook/run-game-thread-work) |
| Open a panel without writing C++ | [Open a panel from Papyrus](cookbook/from-papyrus) |
| Translate my UI | [Translate your interface](cookbook/translate-your-ui) |
| Put a view on a screen in the world | [Put a view on an in-world screen](cookbook/in-world-screen) |
| Save settings from a page | [Build a settings page](cookbook/settings-panel) |
| Capture the page console in my log | [Debugging your view](debugging) |
| Suppress a vanilla menu or HUD | [Vanilla UI suppression](vanilla-ui-suppression) |
| Check whether the view is healthy | [View health and watchdog](view-watchdog) |

## Where the API detail lives

Each call has its own page under [API reference](api-reference) with the signature, parameters, return values and a usage example. Every method page links back to the recipe that uses it, so starting from either direction works.

## Where to go next

- [Cookbook](/docs/cookbook/) — all recipes
- [API reference](api-reference) — every call
- [Debugging your view](debugging) — when the example does not behave
- [Packaging your mod](packaging) — ship it
