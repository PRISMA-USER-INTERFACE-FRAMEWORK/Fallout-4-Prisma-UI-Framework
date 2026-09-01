# PrismaUI_F4 2.1.0 Examples

These examples target the released **PrismaUI_F4 2.1.0** public API and the in-process **Ultralight 1.4.0** backend.

They intentionally avoid the retired legacy-runtime host, previous browser runtime DevTools, remote-CDN dependencies, and unsupported Next-Gen runtime claims.

## 1. Minimal cooperative panel

```cpp
#include "PrismaUI_F4_API.h"

static PRISMA_UI_API::IVPrismaUI10* g_api = nullptr;
static PrismaView g_view = 0;

static void ClosePanel()
{
    if (!g_api || !g_api->IsValid(g_view)) return;
    g_api->Unfocus(g_view);
    g_api->Hide(g_view);
}

static void OnCloseRequested(const char*)
{
    ClosePanel();
}

static void OnDomReady(PrismaView view)
{
    g_api->RegisterJSListener(view, "requestClose", OnCloseRequested);
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
    g_api->Focus(g_view, true, false);
}
```

```html
<button id="close" type="button">Close</button>
<script>
  document.querySelector('#close').addEventListener('click', () => {
    window.requestClose();
  });
</script>
```

## 2. Push structured game data to JavaScript

```cpp
std::string json = R"({"hp":210,"ap":75,"name":"Sole Survivor"})";
g_api->InteropCall(g_view, "setPlayerState", json.c_str());
```

```js
window.setPlayerState = function (json) {
  const state = JSON.parse(json);
  document.querySelector('#hp').textContent = String(state.hp);
  document.querySelector('#ap').textContent = String(state.ap);
};
```

`InteropCall` is the normal choice when calling a named function. Use `Invoke` for one-off evaluated JavaScript.

## 3. Receive events from JavaScript

```cpp
static void OnItemSelected(const char* value)
{
    const uint32_t formId = value ? std::strtoul(value, nullptr, 16) : 0;
    if (auto* form = RE::TESForm::GetFormByID(formId)) {
        logger::info("selected {}", form->GetFullName());
    }
}

static void OnDomReady(PrismaView view)
{
    g_api->RegisterJSListener(view, "selectItem", OnItemSelected);
}
```

```js
document.querySelectorAll('[data-formid]').forEach((row) => {
  row.addEventListener('click', () => window.selectItem(row.dataset.formid));
});
```

Prisma-delivered listener callbacks are marshalled onto the game thread in 2.1.0.

## 4. Capture JavaScript console output

```cpp
static void OnConsole(
    PrismaView view,
    PRISMA_UI_API::ConsoleMessageLevel level,
    const char* message)
{
    logger::info("[Prisma JS][{}][{}] {}",
                 view,
                 static_cast<int>(level),
                 message ? message : "");
}

// After the view exists:
g_api->RegisterConsoleCallback(g_view, OnConsole);
```

This is the supported in-game debugging path. The V1 inspector methods remain in the ABI but are unsupported by the 2.1.0 Ultralight backend.

## 5. Translations

`RegisterTranslations` is a runtime JavaScript injection in 2.1.0. Call it from DOM-ready before invoking page code that needs `window.t`.

```cpp
static void OnDomReady(PrismaView view)
{
    g_api->RegisterTranslations(view, "MyPlugin_F4");
    g_api->Invoke(view, "window.onTranslationsReady && window.onTranslationsReady()");
}

g_view = g_api->CreateView("MyPlugin/panel.html", OnDomReady);
```

Translation file:

```text
Data/Interface/Translations/MyPlugin_F4_en.txt
```

Page:

```js
window.onTranslationsReady = function () {
  const t = (key) => window.t?.(key) ?? key;
  document.querySelector('#title').textContent = t('$UI_TITLE');
};
```

Do not assume `window.t` exists in module-level scripts that execute before DOM-ready.

## 6. Passive HUD plus interactive panel

```cpp
static PrismaView g_hud = 0;
static PrismaView g_menu = 0;

void CreateViews()
{
    g_hud = g_api->CreateView("MyPlugin/hud.html", nullptr);
    g_api->SetViewRole(g_hud, PRISMA_UI_API::ViewRole::kWidget);
    g_api->SetOrder(g_hud, 0);

    g_menu = g_api->CreateView("MyPlugin/menu.html", OnDomReady);
    g_api->SetViewRole(g_menu, PRISMA_UI_API::ViewRole::kPanel);
    g_api->SetOrder(g_menu, 10);
    g_api->Hide(g_menu);
}
```

The HUD does not make `IsAnyPanelVisible()` permanently true because it is declared `kWidget`.

## 7. Selective V10 overlay input

```cpp
const bool hasRegions = PRISMA_UI_API::HasPrismaCapability(
    PRISMA_UI_API::PrismaCapability::InputRegions);

if (hasRegions) {
    const PRISMA_UI_API::InputRegion regions[] = {
        { 40, 40, 400, 240 },
        { 500, 40, 120, 48 },
    };

    if (g_api->SetInputRegions(g_view, regions, 2)) {
        g_api->Show(g_view);
        g_api->FocusOverlay(g_view, false, false);
    }
}
```

Clear the mask with:

```cpp
g_api->SetInputRegions(g_view, nullptr, 0);
```

## 8. Native networking -> Prisma view

Do required networking in native code, not in the page:

```cpp
void OnNetworkMessage(std::string payload)
{
    F4SE::GetTaskInterface()->AddTask([payload = std::move(payload)] {
        if (!g_api || !g_api->IsValid(g_view)) return;
        g_api->InteropCall(g_view, "onNetworkMessage", payload.c_str());
    });
}
```

```js
window.onNetworkMessage = function (json) {
  const message = JSON.parse(json);
  renderMessage(message);
};
```

This is the supported pattern for HTTP/WebSocket/server-push features.

## 9. ModelPreview v4

The released bridge is `window.__prismaUI_modelPreview`. It takes NIF paths, not FormIDs.

```js
const preview = window.__prismaUI_modelPreview;

if (preview?.version >= 4) {
  preview.onStatus((status) => {
    if (!status.ok) {
      console.warn('preview failed', status.error);
    }
  });

  preview.show({
    id: 'selection',
    nifPath: 'Meshes/MyMod/item.nif',
    formType: 'MISC',
    x: 600,
    y: 120,
    w: 320,
    h: 320,
    zoom: 1.0
  });
}
```

When the panel closes:

```js
preview?.hide({ id: 'selection' });
```

See [Model Preview](model-preview) for clipping, multi-NIF requests, pose fields, status phases, and performance rules.

## 10. View health diagnostics

```cpp
using VH = PRISMA_UI_API::ViewHealth;

if (g_api && g_api->IsValid(g_view)) {
    switch (g_api->GetViewHealth(g_view)) {
    case VH::kDomReady:
    case VH::kLive:
        break;
    case VH::kLoadFailed:
    case VH::kDomReadyTimeout:
        logger::error("Prisma view failed to load");
        break;
    case VH::kJsError:
        logger::warn("Prisma view reported a JS error");
        break;
    default:
        break;
    }
}
```

Use `PrismaUI_F4.log` and `RegisterConsoleCallback` for the actual failure detail.

## 11. Vanilla menu replacement

```cpp
g_api->SuppressVanillaMenu("ContainerMenu", true);
g_api->CloseVanillaMenu("ContainerMenu");

if (!g_api->IsAnyPanelVisible(g_view)) {
    g_api->Show(g_view);
    g_api->Focus(g_view, true, false);
}
```

When your replacement is disabled:

```cpp
g_api->Unfocus(g_view);
g_api->Hide(g_view);
g_api->SuppressVanillaMenu("ContainerMenu", false);
```

The public API has no `IsMenuSuppressed` method. Track your own requested state if needed.

## 12. Runtime-safe HUD suppression

```cpp
if (!g_api->SuppressHUDWidget("HUDCompass", true)) {
    logger::warn("HUDCompass suppression not available on this runtime/configuration");
}
```

The 2.1.0 implementation has guarded support on OG `1.10.163` and AE `1.11.137+` where the required Address Library authority resolves.

## 13. Do not use these old examples

Do not copy older examples that tell you to:

- create or toggle Chrome/legacy-runtime DevTools with `CreateInspectorView`;
- install `retired host component` or a `retired runtime directory/` tree;
- use `retired runtime library`;
- assume a separate legacy-runtime renderer process isolates JavaScript performance from the game;
- load required Google Fonts/jsDelivr resources remotely;
- support Fallout `1.10.980-1.10.984` just because CommonLib can compile a slot for it;
- use `SuppressActivateChoicePerk` as if it currently filters choices;
- use `dropDefaultTake=true` as if it currently removes the default Take row;
- use `window.prismaModelPreview` / `window.onModelPreviewStatus` or a FormID-based ModelPreview request;
- assume `RegisterTranslations` makes `window.t` available before DOM-ready.

Those are stale assumptions, not the PrismaUI_F4 2.1.0 contract.

## Related pages

- [Quick Start](quick-start)
- [API Reference](api-reference)
- [View Lifecycle](view-lifecycle)
- [Networking](networking)
- [Model Preview](model-preview)
- [Translations](translations)
- [Vanilla UI Suppression](vanilla-ui-suppression)
