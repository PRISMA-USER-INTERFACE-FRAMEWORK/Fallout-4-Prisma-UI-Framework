---
title: 'Limitations'
---
# Limitations

PrismaUI_F4 2.2.0 uses **Ultralight 1.4.0 in-process**. The retired legacy-runtime host/subprocess architecture is not the production runtime, so previous browser runtime-specific guarantees from older documentation do not apply.

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

For the current shipping policy:

- bundle required JS, CSS, fonts, images, and other assets with the mod;
- do not depend on CDNs or remote fonts;
- Worker, SharedWorker, and ServiceWorker-style behavior is blocked by the Prisma sandbox;
- do not design required functionality around direct page `fetch`, XHR, WebSocket, or private-network access;
- implement real network I/O in native C++ and relay validated data into the view.

See [Networking](networking).

## Rendering

PrismaUI can use the validated D3D11 GPU-accelerated Ultralight path when the graphics environment is eligible. CPU BitmapSurface rendering remains a controlled fallback.

Consumer mods should not assume which path is active. Keep repaint regions bounded, avoid unnecessary full-screen animation, and test with the graphics wrappers your mod supports.

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

The old external legacy-runtime/Chrome debugging workflow is retired.

Current PrismaUI builds include a packaged in-game Ultralight inspector opened with **F12** when DevTools are enabled. Use it for Elements, Sources, Console, Network, Timelines, Storage, Graphics, Layers, and Audit inspection.

The V1 inspector methods are compatibility ABI and should not be treated as the control surface for the current F12 DevTools workflow. Keep framework logs and `RegisterConsoleCallback` in your debugging workflow as well.

## Runtime-specific Fallout features

Not every Fallout-facing helper has identical implementation requirements across game versions.

The current desktop line supports:

- OG 1.10.163;
- AE 1.11.137+ when matching Address Library data is available.

The intermediate 1.10.980-1.10.984 Next-Gen line is deliberately rejected.

For example, `SuppressHUDWidget` uses validated runtime/address authorities and can fail closed if the required runtime data cannot be resolved. Always check return values on APIs that provide them.

## Activate-choice compatibility calls

Two older V7 methods need special attention:

- `EnableActivateChoiceFilter(enable, dropDefaultTake)` enables the capture path used by the V8 read/trigger APIs. `dropDefaultTake` is retained for ABI compatibility and ignored.
- `SuppressActivateChoicePerk(...)` is an ABI placeholder. Perk-row filtering is not implemented.

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

The following are historical and should not appear in a current installation guide or troubleshooting checklist:

- `retired host component`;
- `retired subprocess`;
- `retired runtime directory/` production runtime tree;
- `retired runtime library`;
- shared legacy-runtime-shell deployment steps;
- shim/host ABI mismatch troubleshooting.

If a player's installation contains a mixture of retired legacy-runtime files and current Ultralight files, reinstall the current PrismaUI package cleanly rather than trying to combine the two runtime generations.
