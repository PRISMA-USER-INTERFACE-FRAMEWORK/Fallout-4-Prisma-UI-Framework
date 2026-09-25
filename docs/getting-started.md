# Getting Started with PrismaUI_F4

This guide takes an F4SE C++ plugin from zero to a working PrismaUI panel using the preferred modern feature-table API. The numbered `PrismaUI_F4_API.h` V1-V12 surface remains the compatibility ABI for existing consumers.

PrismaUI uses Ultralight 1.4.0 in-process. Desktop support covers Fallout 4 `1.10.163` (OG) and `1.11.137+` (AE) with matching Address Library data. The intermediate `1.10.980-1.10.984` line is not supported.

## 1. Install the requirements

You need Visual Studio 2022 with C++, Git, your normal F4SE/CommonLibF4 setup, and PrismaUI installed as a mod.

The runtime contains `PrismaUI_F4.dll` plus the Ultralight runtime under `Data/PrismaUI_F4/`.

## 2. Copy the preferred API header

For new integrations, copy and include:

```text
src/PrismaUI_F4_Modern_API.h
```

```cpp
#include "PrismaUI_F4_Modern_API.h"

using namespace PRISMA_UI_FLAT_API;
```

Use `src/PrismaUI_F4_API.h` only when maintaining the stable numbered V1-V12 compatibility ABI.

## 3. Discover feature tables

Request only the contracts the plugin needs and null-check discovery through its boolean result.

```cpp
static ViewAPI g_viewApi{};
static ControllerAPI g_controller{};
static InteropAPI g_interop{};
static GameThreadAPI g_gameThread{};

static bool LoadPrisma()
{
    return Discover<ApiFeature::View>(ViewApiVersion, g_viewApi) &&
           Discover<ApiFeature::Controller>(ControllerApiVersion, g_controller) &&
           Discover<ApiFeature::Interop>(InteropApiVersion, g_interop) &&
           Discover<ApiFeature::GameThread>(GameThreadApiVersion, g_gameThread);
}
```

Call `LoadPrisma()` during or after `kGameDataReady`. Do not infer support from a PrismaUI version string when feature discovery is available.

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
    g_controller.BindControllerAction(view, "X", "panel.secondary");
    g_controller.BindControllerAction(view, "Y", "panel.favorite");
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

`CreateView` begins visible, so normal panels usually hide immediately. Controller bindings belong in the DOM-ready path. PrismaUI 2.2.0 owns controller bridge installation and bounded recovery.

## 5. Bind page events

```cpp
static void OnDomReady(PrismaView view)
{
    g_interop.RegisterJSListener(view, "closePanel", [](const char*) {
        if (!g_viewApi.IsValid(g_view)) return;
        g_viewApi.Unfocus(g_view);
        g_viewApi.Hide(g_view);
    });

    g_controller.BindControllerAction(view, "X", "panel.secondary");
}
```

The page can receive controller actions with the `prisma-controller-action` DOM event.

## 6. Mutate Fallout state through GameThreadAPI

If page code must mutate Fallout-owned state, bind a verified game-thread event:

```cpp
static void OnApply(const char*, void*)
{
}

static void OnDomReady(PrismaView view)
{
    g_gameThread.BindGameThreadUIEvent(view, "applySetting", OnApply, nullptr);
}
```

Or dispatch native work explicitly with `DispatchToGameThread`. A rejected dispatch returns `false` and will not execute later.

## 7. Show and focus the panel

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

For selective overlay input, discover `InputAPI`. For menu/HUD operations, discover `MenuAPI`.

## 8. Send data between C++ and JavaScript

```cpp
g_interop.InteropCall(g_view, "setStatus", R"({"ready":true})");
```

Use `InteropAPI` for `Invoke`, `InteropCall`, JS listeners, UI events, and console callbacks.

## 9. Use V4 localization for new integrations

Discover `LocalizationAPI` and call `RegisterTranslationsV4` from the document-ready path.

```cpp
LocalizationAPI localization{};
if (Discover<ApiFeature::Localization>(LocalizationApiVersion, localization)) {
    localization.RegisterTranslationsV4(g_view, "MyPlugin");
}
```

See [Translations](translations) for the JSON file contract, Fallout resource loading, locale fallback, dotted keys, and interpolation.

## 10. Bundle web dependencies locally

Do not make a shipped view depend on CDNs, Google Fonts, or retired browser-runtime behavior. Bundle required HTML, JS, CSS, fonts, and images with your mod. Implement real HTTP/WebSocket/authentication work in native C++ and relay validated data to the page.

## 11. Debugging

Use:

- the packaged F12 Ultralight inspector when DevTools are enabled;
- `PrismaUI_F4.log`;
- your plugin log;
- `InteropAPI::RegisterConsoleCallback`;
- `ViewAPI::GetViewHealth`.

## Before you ship

Verify that your plugin:

- uses `PrismaUI_F4_Modern_API.h` for new feature-table integrations;
- discovers and checks every feature table it calls;
- binds controller actions only after DOM-ready;
- routes engine mutations through `GameThreadAPI` or other correct native ownership;
- handles focus/unfocus deliberately;
- keeps required assets local;
- tests each Fallout runtime family it claims to support.

For existing numbered-interface code, see [API Reference V1-V12](api-reference) and [Current API Extensions](api-extensions).

Next: [Modern API](modern-api), [Controller Actions](controller-actions), [Translations](translations), [View Lifecycle](view-lifecycle), [Networking](networking), and [Troubleshooting](troubleshooting).
