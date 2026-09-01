---
title: 'Limitations'
---
# Limitations

PrismaUI_F4 2.1.0 uses **Ultralight 1.4.0 in-process**. The retired legacy-runtime host/subprocess architecture is not the production runtime, so previous browser runtime-specific guarantees from older documentation do not apply.

The safest rule for consumer mods is simple: use ordinary HTML/CSS/JavaScript, bundle required assets locally, and test every browser feature your UI depends on against Ultralight 1.4.0.

## Web-platform compatibility

Ultralight is not a drop-in previous browser runtime/Electron environment. Do not assume that a feature works only because it works in Chrome.

Before shipping, test any dependency on:

- newer or uncommon CSS features;
- advanced media behavior;
- browser-specific JavaScript APIs;
- framework/polyfill behavior;
- font rendering and fallback behavior;
- canvas or other rendering APIs;
- focus and keyboard-event edge cases.

React, Vue, Svelte, Tailwind, and similar stacks are usable when their **built output** works under Ultralight 1.4.0. PrismaUI does not guarantee every current browser framework release indefinitely.

## Networking

A Prisma view is intended to be a local UI document, not an unrestricted network client.

For the shipping 2.1.0 policy:

- bundle required JS, CSS, fonts, images, and other assets with the mod;
- do not depend on CDNs or remote fonts;
- Worker, SharedWorker, and ServiceWorker-style behavior is blocked by the Prisma sandbox;
- do not design required functionality around direct page `fetch`, XHR, WebSocket, or private-network access;
- implement real network I/O in native C++ and relay validated data into the view.

See [Networking](networking).

## Rendering

The public 2.1.0 configuration uses the **CPU BitmapSurface presentation path**. Accelerated Ultralight rendering remains internal/deferred and should not be assumed by consumer mods.

Practical consequences:

- large areas that repaint continuously cost more than mostly-static UI;
- avoid unnecessary full-screen animation and expensive effects when a smaller animated region will do;
- hidden/offscreen views can still incur work depending on how you configure them;
- performance-sensitive on-mesh views should use an appropriate offscreen size rather than rendering far above the target surface resolution.

Consumer code must not depend on the internal accelerated renderer being enabled.

## Offscreen and on-mesh rendering

V5 exposes offscreen rendering and texture-handoff APIs, but engine-side mesh binding is a more fragile integration surface than normal screen overlays.

The framework ships with `bMeshBinding=0` by default. Treat mesh-binding features as opt-in and test the exact game/runtime/content combination you support.

For a view used offscreen:

- set the offscreen configuration before relying on its texture;
- match `SetViewOffscreenSize` to the target surface's aspect ratio and useful resolution;
- rebind when the target game geometry is rebuilt;
- unbind while the original geometry is still valid;
- do not retain or release the framework-owned SRV yourself.

## Inspector / DevTools

Do not follow earlier-runtime instructions that expect an external previous browser runtime DevTools session or a `retired host component` inspector window.

The older inspector entry points remain in the ABI for compatibility, but 2.1.0 consumer debugging should primarily use:

- `PrismaUI_F4.log`;
- your plugin's F4SE log;
- `RegisterConsoleCallback`;
- `GetViewHealth`;
- normal browser/front-end testing outside the game.

`SetInspectorBounds` should not be treated as a meaningful 2.1.0 layout API.

## Runtime-specific Fallout features

Not every Fallout-facing helper has identical implementation requirements across game versions.

Desktop 2.1.0 supports:

- OG 1.10.163;
- AE 1.11.137+ when matching Address Library data is available.

The intermediate 1.10.980-1.10.984 Next-Gen line is deliberately rejected.

For example, `SuppressHUDWidget` uses validated runtime/address authorities and can fail closed if the required runtime data cannot be resolved. Always check return values on APIs that provide them.

## Activate-choice compatibility calls

Two older V7 methods need special attention in 2.1.0:

- `EnableActivateChoiceFilter(enable, dropDefaultTake)` enables the capture path used by the V8 read/trigger APIs. `dropDefaultTake` is retained for ABI compatibility and ignored.
- `SuppressActivateChoicePerk(...)` is an ABI placeholder. Perk-row filtering is not implemented in 2.1.0.

Do not build gameplay behavior around the older filtering description.

## V10 input regions

`IVPrismaUI10` exposes `FocusOverlay` and `SetInputRegions`, but selective input-region behavior is capability-gated.

Check:

```cpp
PRISMA_UI_API::HasPrismaCapability(
    PRISMA_UI_API::PrismaCapability::InputRegions)
```

before depending on it. Fall back to normal `Focus()` when appropriate.

## Multiple views

Multiple views are supported, but creating one view per screen is not automatically the best architecture.

Prefer a single application view with internal routing when the screens are really one UI. Separate views make sense for genuinely independent surfaces such as a persistent HUD widget plus a separate interactive panel.

For views that take input, declare `ViewRole::kPanel` or another appropriate role so other PrismaUI mods can coordinate through `IsAnyPanelVisible()` and `GetFocusedView()`.

## Input quirks

Game input and browser-like text input can interact in ways that are specific to Fallout and the current input route. Test:

- keyboard and mouse;
- controller input if supported by your mod;
- Escape ownership;
- focus changes;
- hide/show/destroy while focused;
- alt-tab/minimize/restore;
- text entry for keys also bound to gameplay actions.

Use V10 input regions only for selective overlay interaction, and keep their rectangles synchronized with the actual UI layout.

## Custom cursor

A custom cursor image can be installed at:

```text
Data/PrismaUI_F4/misc/cursor.png
```

Test it against bright and dark game backgrounds and at the resolutions your mod supports.

## Removed legacy-runtime architecture

The following are historical and should not appear in a 2.1.0 installation guide or troubleshooting checklist:

- `retired host component`;
- `retired subprocess`;
- `retired runtime directory/` production runtime tree;
- `retired runtime library`;
- shared legacy-runtime-shell deployment steps;
- shim/host ABI mismatch troubleshooting.

If a player's installation contains a mixture of retired legacy-runtime files and current Ultralight files, reinstall the current 2.1.0 package cleanly rather than trying to combine the two runtime generations.
