---
title: 'Modern Frameworks'
---
# Modern Frameworks

React, Vue, Svelte, Solid, and similar frameworks can be used with **PrismaUI_F4 2.1.0** when you ship the result as compatible local HTML/CSS/JavaScript assets.

The in-game runtime is **Ultralight 1.4.0**, not previous browser runtime. Test the actual production bundle under PrismaUI and avoid making required UI behavior depend on a browser feature you have only tested in Chrome.

## The bridge timing problem

Your C++ DOM-ready callback can run before framework components finish mounting. If C++ calls a JavaScript function that a React/Vue/Svelte component has not registered yet, that first message can be lost.

A robust pattern is:

1. expose C++-callable `window.*` functions at JavaScript module-load time;
2. let those functions write into a framework-agnostic store;
3. let components subscribe to that store;
4. from Prisma's DOM-ready callback, register JS-to-C++ listeners and push initial data.

## React example

```ts
// store.ts
import { create } from 'zustand';

type PlayerState = {
  health: number;
  maxHealth: number;
};

type Store = {
  player: PlayerState | null;
  setPlayer: (player: PlayerState) => void;
};

export const useGameStore = create<Store>((set) => ({
  player: null,
  setPlayer: (player) => set({ player }),
}));

window.setPlayerState = (json: string) => {
  useGameStore.getState().setPlayer(JSON.parse(json));
};
```

Import that module before rendering the application:

```tsx
import './store';
import ReactDOM from 'react-dom/client';
import App from './App';

ReactDOM.createRoot(document.getElementById('root')!).render(<App />);
```

C++ can then push state after DOM-ready:

```cpp
static void ClosePanel(const char*)
{
    if (!g_api || !g_view) return;
    g_api->Unfocus(g_view);
    g_api->Hide(g_view);
}

static void OnDomReady(PrismaView view)
{
    g_api->RegisterJSListener(view, "closePanel", ClosePanel);
    g_api->InteropCall(
        view,
        "setPlayerState",
        R"({"health":100,"maxHealth":100})");
}
```

## JavaScript to C++

Register page-facing listeners from the DOM-ready path. Prisma-delivered JS listener callbacks are marshalled to the game thread in 2.1.0.

```cpp
static void SaveSettingsFromJS(const char* json)
{
    if (!json) return;
    SaveSettings(json);
}

static void OnDomReady(PrismaView view)
{
    g_api->RegisterJSListener(view, "saveSettings", SaveSettingsFromJS);
}
```

```ts
export function save(settings: object) {
  window.saveSettings(JSON.stringify(settings));
}
```

## TypeScript declarations

```ts
export {};

declare global {
  interface Window {
    setPlayerState: (json: string) => void;
    closePanel: () => void;
    saveSettings: (json: string) => void;
    onTranslationsReady?: () => void;
    t?: (key: string) => string;
    L10N?: Record<string, string>;
  }
}
```

## Translations with a framework app

`RegisterTranslations` is a runtime `Invoke` in 2.1.0. It does **not** make `window.t` available before your bundle's module-level code executes.

Register translations in DOM-ready, then call an application hook that reads them:

```cpp
static void OnDomReady(PrismaView view)
{
    g_api->RegisterTranslations(view, "MyPlugin_F4");
    g_api->Invoke(view, "window.onTranslationsReady && window.onTranslationsReady()");
}
```

```ts
window.onTranslationsReady = () => {
  const t = (key: string) => window.t?.(key) ?? key;
  // Update your store or framework state here.
};
```

Do not build module-level constants from `window.t` unless your own bootstrap code guarantees that translation registration has already happened.

## Development outside Fallout

Use your normal Vite/Webpack/dev-server workflow for layout and application logic, with mock data when Prisma is absent.

```ts
if (import.meta.env.DEV) {
  window.setPlayerState(JSON.stringify({ health: 85, maxHealth: 100 }));
}
```

The browser preview is a development convenience, not proof that an API is available in Ultralight 1.4.0. Feature-detect anything outside the tested baseline.

## Bundle dependencies locally

Do not ship a production page that requires:

- Google Fonts;
- jsDelivr/unpkg;
- a remote JavaScript module;
- a remote stylesheet;
- browser WebSocket/Worker infrastructure.

Bundle those files with the mod instead.

A typical Vite build can emit:

```text
Data/PrismaUI_F4/views/MyPlugin/
├── index.html
└── assets/
    ├── app.js
    ├── app.css
    └── ui.woff2
```

If the mod needs real networking, keep that in native C++ and bridge the resulting data into the view.

## Multiple screens

You do not have to use exactly one Prisma view per plugin. Use one routed frontend when screens share lifecycle/state, and multiple views when they are genuinely separate surfaces such as a persistent HUD plus an independent panel.

For V10 projects:

- mark passive HUDs `ViewRole::kWidget`;
- mark interactive menus `ViewRole::kPanel`;
- use `IsAnyPanelVisible` before opening a cooperating panel;
- capability-check `InputRegions` before using `FocusOverlay` / `SetInputRegions`.

## Key rules

| Rule | Reason |
|---|---|
| Expose C++-called `window.*` functions at module load | They exist before components mount |
| Register Prisma JS listeners at DOM-ready | Avoid view/context timing ambiguity |
| Register translations at DOM-ready before translation-dependent init | 2.1.0 injects them through `Invoke` |
| Use `InteropCall` for normal named C++ -> JS messages | Simple structured bridge path |
| Use JSON strings for structured payloads | Stable language boundary |
| Bundle required web dependencies locally | Matches the 2.1.0 sandbox and offline mod packaging |
| Test the built bundle in Ultralight | Chrome/Vite success is not runtime proof |

See [Translations](translations), [HTML Views](html-views), [Networking](networking), [View Lifecycle](view-lifecycle), and [API Reference](api-reference).
