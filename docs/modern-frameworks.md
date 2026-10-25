---
title: 'Modern frameworks'
---

# Modern frameworks

React, Vue, Svelte, Solid and friends work with PrismaUI — as long as you ship the built result as local HTML, CSS and JavaScript.

The page runs in Ultralight 1.4.0, not in Chrome. Your browser dev server is useful; it is not proof that the runtime supports something.

## The one real problem: bridge timing

Your C++ `OnDomReady` can run before your components have mounted. If C++ calls a `window.something` that a component registers on mount, that first message is lost.

The fix is a small pattern:

1. register C++-callable `window.*` functions at **module load**, before the app renders;
2. have them write into a framework-agnostic store;
3. let components subscribe to the store;
4. register page → C++ listeners and send initial data from `OnDomReady`.

## React example

```ts
// store.ts
import { create } from 'zustand';

type PlayerState = { health: number; maxHealth: number };

type Store = {
  player: PlayerState | null;
  setPlayer: (player: PlayerState) => void;
};

export const useGameStore = create<Store>((set) => ({
  player: null,
  setPlayer: (player) => set({ player }),
}));

// This runs at module load, before React renders.
window.setPlayerState = (json: string) => {
  useGameStore.getState().setPlayer(JSON.parse(json));
};
```

Import the store before rendering:

```tsx
import './store';
import ReactDOM from 'react-dom/client';
import App from './App';

ReactDOM.createRoot(document.getElementById('root')!).render(<App />);
```

Then C++ can push data as soon as the DOM is ready:

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
    g_api->InteropCall(view, "setPlayerState", R"({"health":100,"maxHealth":100})");
}
```

## Page → C++

Register listeners from `OnDomReady`; PrismaUI delivers them on the game thread.

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

Keep the bridge typed once, in a `globals.d.ts`:

```ts
export {};

declare global {
  interface Window {
    setPlayerState: (json: string) => void;
    closePanel: () => void;
    saveSettings: (json: string) => void;
    onTranslationsReady?: () => void;
    PrismaL10N?: { locale: string; messages: Record<string, string>; t: (key: string, vars?: Record<string, unknown>) => string };
    __prismaUI_modelPreview?: {
      version: number;
      show: (options: Record<string, unknown>) => void;
      hide: (options: { id?: string }) => void;
      onStatus?: (callback: (status: Record<string, unknown>) => void) => void;
    };
  }
}
```

## Translations with a framework app

`RegisterTranslationsV4` injects `window.PrismaL10N` into the live document. It is not available before your bundle's module-level code runs.

Register in `OnDomReady`, then call a page hook that reads the translations:

```cpp
static void OnDomReady(PrismaView view)
{
    g_api->RegisterJSListener(view, "closePanel", ClosePanel);
    g_localization.RegisterTranslationsV4(view, "MyPlugin");
    g_api->Invoke(view, "window.onTranslationsReady && window.onTranslationsReady()");
}
```

```ts
window.onTranslationsReady = () => {
  const l10n = window.PrismaL10N;
  const t = l10n ? l10n.t : (key: string) => key;
  // Update your store or framework state here.
};
```

Do not build module-level constants from translations; there is no guarantee they exist yet.

## Developing outside Fallout

Use Vite/Webpack and mock data for layout work:

```ts
if (import.meta.env.DEV) {
  window.setPlayerState(JSON.stringify({ health: 85, maxHealth: 100 }));
}
```

Then test the **production build** in game. Anything outside the tested Ultralight baseline should be feature-detected:

```js
if (typeof ResizeObserver !== 'undefined') {
  // optional enhancement
}
```

## Bundle everything locally

A production page cannot load remote fonts, scripts or styles. Ship them:

```text
Data/PrismaUI_F4/views/MyPlugin/
├── index.html
└── assets/
    ├── app.js
    ├── app.css
    └── ui.woff2
```

If your app needs HTTP or sockets, keep that in C++ and bridge the result into the page. See [Networking](networking).

## One app, several screens

You do not need one Prisma view per screen. Prefer one routed app when the screens share state and lifecycle. Use separate views for genuinely independent surfaces: a persistent HUD plus an independent panel, for example.

For separate views:

- mark HUDs `ViewRole::kWidget`, interactive menus `ViewRole::kPanel`;
- check `IsAnyPanelVisible` before opening a menu;
- capability-check input regions before using overlay focus.

## Rules worth remembering

| Rule | Why |
| --- | --- |
| Register `window.*` handlers at module load | They exist before components mount |
| Register Prisma listeners in `OnDomReady` | The document exists; timing is predictable |
| Register translations before rendering translated text | They arrive as a live injection |
| Send structured data as JSON strings | One stable language boundary |
| Bundle web dependencies locally | The sandbox blocks remote assets |
| Test the built bundle in game | Chrome success is not Ultralight success |

## Where to go next

- [The HTML side](html-views) — the page contract
- [Translations](translations) — V4 JSON files
- [Show a 3D item preview](cookbook/item-preview) — a framework-friendly preview integration
- [Performance notes](html-views) — keep the page cheap
