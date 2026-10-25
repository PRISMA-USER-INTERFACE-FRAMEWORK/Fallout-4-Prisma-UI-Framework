---
title: 'Limitations'
---

:::tip New to PrismaUI?
Start with [Your first panel](quick-start) for a complete walkthrough, or copy a working example from the [Cookbook](/docs/cookbook/).
:::

# Limitations

PrismaUI runs Ultralight 1.4.0 inside Fallout 4. It is a good web engine for game interfaces and it is not Chrome. This page is the honest list of what that means, so you do not build on a feature that is not there.

The short rule: use ordinary HTML, CSS and JavaScript, ship your assets with the mod, and test anything unusual in game before you rely on it.

## Browser features

Do not assume a feature works because it works in your desktop browser. Test anything you depend on, especially:

- newer or uncommon CSS (container queries, `:has()`, subgrid, view transitions);
- media playback and streaming;
- browser-only JavaScript APIs (`IntersectionObserver`, `ResizeObserver` are worth checking in your target build);
- large framework code paths that assume modern Chrome;
- font fallback and variable fonts;
- canvas and WebGL usage;
- focus, selection and keyboard event edge cases.

React, Vue, Svelte and Tailwind are usable when their built output works in Ultralight 1.4.0. PrismaUI does not pin every framework version forever — test your build.

## Networking

A view is a local document, not a network client:

- no remote scripts, styles or fonts;
- no `fetch`/XHR to the internet;
- no WebSockets, EventSource, Workers or WebRTC;
- no localhost or LAN access from page code.

Do network work in C++ and pass the result to the page. See [Networking](networking).

## Rendering paths

PrismaUI uses its validated GPU path when the graphics environment is eligible and falls back to CPU rendering when it is not. Your page should not care which one is active, but it should not assume unlimited headroom:

- keep repaint areas small;
- avoid full-screen animation for small changes;
- do not run heavy per-frame work in JavaScript;
- test with the graphics tools your players use (ENB, frame generation, overlays).

If another graphics injector owns presentation in a way PrismaUI cannot safely share, PrismaUI may skip its own draw rather than risk the game. In that case the UI may not appear until the conflict is resolved — that is a deliberate fail-closed choice, not a random glitch.

## Offscreen and mesh-bound views

Offscreen rendering and binding a view to a mesh are advanced. They are more fragile than a normal screen overlay and depend on the exact game version and content.

- mesh binding is off by default;
- only verified runtime layouts are allowed, and unsupported ones fail closed;
- set the offscreen size before using the texture;
- the framework owns the texture view — do not retain or release it yourself;
- rebind if the game rebuilds the target geometry.

If you are building a normal menu or HUD, you do not need any of this.

## Inspector and DevTools

The old external browser debugging workflow is gone. Current builds package an in-game inspector, opened with **F12** when DevTools are enabled.

The V1 inspector methods are compatibility ABI; they are not how you control the F12 inspector. Keep the framework log and `RegisterConsoleCallback` in your debugging loop.

## Game-version support

Supported desktop runtimes:

- Old-Gen **1.10.163**;
- Anniversary Edition **1.11.137 and newer**, with matching game support data installed.

The 1.10.980–1.10.984 line is deliberately rejected. Features that need version-specific engine data fail closed when that data is missing instead of guessing.

Some helper calls are narrower than the general framework:

- `SuppressHUDWidget` needs a supported runtime with matching data, and returns `false` otherwise;
- native controller conversion is validated only for Old-Gen 1.10.163 and Anniversary Edition 1.11.240;
- game-thread dispatch (V11) and controller actions (V12) exist on the flat provider, not on VR.

Check the specific API page when the runtime matters.

## Legacy compatibility quirks

Two V7 calls behave differently from older guides:

- `EnableActivateChoiceFilter(enable, dropDefaultTake)`: `dropDefaultTake` is ignored; it only enables capture.
- `SuppressActivateChoicePerk`: a compatibility placeholder that does not filter perk rows.

Use `GetActivateChoiceLabel` and `TriggerActivateChoice` for the supported path.

## Input regions are optional

`FocusOverlay` and `SetInputRegions` (V10) only work when the loaded build enables input regions. Check the capability first:

```cpp
if (!PRISMA_UI_API::HasPrismaCapability(PRISMA_UI_API::PrismaCapability::InputRegions)) {
    // Fall back to a normal focused panel.
}
```

`IVPrismaUI10` existing does not prove the behaviour is enabled.

## Multiple views

Multiple views work, but they are not automatically the right architecture. Prefer one view with internal page routing when the screens are one UI. Use separate views for genuinely independent surfaces: a persistent HUD plus a separate menu, for example.

Declare a role (`kPanel` / `kWidget`) on any view so other Prisma mods can coordinate. See [Panel management](panel-management).

## Input quirks worth testing

Game input and page text input interact in engine-specific ways. Test:

- keyboard and mouse in menus and in text fields;
- controller input, if you use it;
- Escape ownership;
- focus changes while typing;
- hide/show/destroy while focused;
- Alt-Tab and window restore;
- keys that are also bound to gameplay actions.

## Custom cursor

A custom cursor image can be supplied at:

```text
Data/PrismaUI_F4/misc/cursor.png
```

Check it against bright and dark backgrounds and at the resolutions you support.

## What is not in the current runtime

If you find an old guide that mentions a separate browser host process, a subprocess, a runtime directory tree or a host/subprocess ABI mismatch: that architecture is retired and none of it belongs in a current install. If a player's game folder still has a mix of old and current files, reinstall the package cleanly.

## Where to go next

- [Networking](networking) — the network rules in detail
- [Troubleshooting](troubleshooting) — symptom-by-symptom
- [FAQ](faq) — short answers
- [API reference](api-reference) — per-call behaviour and return values
