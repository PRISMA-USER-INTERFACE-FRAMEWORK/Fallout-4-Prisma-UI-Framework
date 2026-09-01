---
id: troubleshooting
title: Troubleshooting
sidebar_label: Troubleshooting
sidebar_position: 99
---

# Troubleshooting PrismaUI_F4 2.1.0

Start with:

```text
Documents\My Games\Fallout4\F4SE\PrismaUI_F4.log
```

Also keep your consumer plugin log. For JavaScript errors, register `RegisterConsoleCallback` during development.

## Nothing appears on screen

Check these in order:

1. `PrismaUI_F4.dll` is installed under `Data/F4SE/Plugins/` and actually loaded by F4SE.
2. The current Ultralight runtime tree exists under `Data/PrismaUI_F4/` with its `libs`, `resources`, and runtime data.
3. `RequestPluginAPI` is called during/after `kGameDataReady` and its return value is not null.
4. The HTML path is relative to `Data/PrismaUI_F4/views/`.
5. The view handle is nonzero and `IsValid(view)` is true.
6. You did not hide the view and forget to show it again.

`CreateView` starts a view visible. Many plugins intentionally call `Hide(view)` immediately after creation so the panel can be preloaded. If you do that, call `Show(view)` on the real open path.

## The UI appears but input does not work

A visible view does not automatically mean it owns interactive focus.

For a conventional panel:

```cpp
api->SetViewRole(view, PRISMA_UI_API::ViewRole::kPanel);
api->Show(view);
api->Focus(view, false, false);
```

Before opening a panel over other PrismaUI mods, use `IsAnyPanelVisible()` or `GetFocusedView()` rather than treating every visible widget as a blocker.

For selective overlay input, use `FocusOverlay`/`SetInputRegions` only when:

```cpp
PRISMA_UI_API::HasPrismaCapability(
    PRISMA_UI_API::PrismaCapability::InputRegions)
```

returns true.

## A JS listener does nothing

Check:

- the C++ listener name and JavaScript function name match exactly;
- the view is valid and its document has reached the expected lifecycle point;
- your JavaScript did not throw before calling the listener;
- `RegisterConsoleCallback` for console errors;
- `GetViewHealth(view)` for load/unresponsive state.

Use `BindUIEvent` when you want the game-thread callback contract to be explicit.

## The game crashes when calling PrismaUI

The most common consumer-side cause is dereferencing a null or incompatible API pointer.

```cpp
auto* api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI10>();
if (!api) {
    logger::error("PrismaUI_F4 V10 unavailable");
    return;
}
```

Compile against the current public 2.1.0 header. Do not mix an old copied header with assumptions about newer vtable methods.

The docs-repo header is mirrored from the release, and `prisma-mcp get_header` reads the exact 2.1.0 header directly from the released Prisma-Matrix source commit.

## The UI is blank or unstyled

Bundle required assets with the mod and use document-relative paths:

```html
<link rel="stylesheet" href="app.css">
<script src="app.js"></script>
```

Do not depend on CDN JavaScript, Google Fonts, or other remote resources for the production 2.1.0 UI path. The shipping Ultralight security model is intentionally restrictive.

If a remote dependency worked under an old legacy-runtime build and fails now, move it into your mod.

See [Networking](networking).

## DevTools / inspector instructions from old guides do not work

PrismaUI_F4 2.1.0 no longer ships the legacy-runtime host/subprocess or the old external previous browser runtime DevTools workflow.

Use:

- `PrismaUI_F4.log`;
- your plugin log;
- `RegisterConsoleCallback`;
- `GetViewHealth`;
- normal browser/front-end testing outside the game.

The old inspector entry points remain in the ABI for compatibility, but do not build your development workflow around legacy-runtime remote debugging or `retired host component`.

## The installation contains old legacy-runtime files

A current 2.1.0 install should not require:

```text
retired host component
retired subprocess
retired runtime library
retired runtime directory/
```

If you upgraded from an old development build and still have mixed runtime generations, reinstall the current PrismaUI_F4 2.1.0 package cleanly rather than trying to combine them.

Do **not** delete the current Ultralight libraries. `AppCore.dll`, `Ultralight.dll`, `UltralightCore.dll`, and `WebCore.dll` are part of the 2.1.0 runtime.

## HUD suppression does not work

`SuppressHUDWidget` in 2.1.0 supports:

- OG 1.10.163;
- AE 1.11.137+ when matching Address Library data is available.

The intermediate 1.10.980-1.10.984 Next-Gen runtime line is not supported by PrismaUI_F4 2.1.0.

The call can also return `false` for an unknown widget class or when the required runtime authority cannot be safely resolved. Check the return value and the framework log.

## Activate-choice filtering does not behave like old docs

The released V7 behavior is intentionally narrower than older documentation claimed:

- `EnableActivateChoiceFilter(true, dropDefaultTake)` enables activate-choice **capture**. `dropDefaultTake` is ignored in 2.1.0.
- `SuppressActivateChoicePerk` is an ABI placeholder and does not remove perk rows.
- Use V8 `GetActivateChoiceLabel` and `TriggerActivateChoice` for the supported capture/read/trigger path.

## Input leaks to the game

If the UI is meant to be a normal interactive panel, use `Focus()` rather than only `Show()`.

If it is meant to be a partially interactive overlay, use V10 input regions and keep the rectangles synchronized with the visible controls. An incorrect region can make empty space consume input or let a visible control click through to the game.

For Escape behavior, review `SetViewOwnsEscape` and make sure your JavaScript provides a close path before taking ownership of Escape.

## 3D/on-mesh view issues

For offscreen/on-mesh views:

- ensure the view has rendered before expecting a live SRV;
- configure offscreen size for the target surface;
- bind after the target geometry exists;
- rebind if Fallout rebuilds that geometry;
- unbind while the original geometry is still valid;
- do not `Release()` the framework-owned SRV;
- remember that public 2.1.0 ships with `bMeshBinding=0` by default.

## Runtime/version confusion

The supported desktop framework line is:

- 1.10.163 OG
- 1.11.137+ AE with matching Address Library data

Do not treat 1.10.980-1.10.984 as a supported middle runtime just because older docs called it "NG".

When reporting a bug, include:

- PrismaUI_F4 version (`2.1.0` for this documentation set);
- Fallout executable version;
- F4SE version;
- Address Library version when relevant;
- `PrismaUI_F4.log`;
- consumer plugin log;
- whether the issue is a normal screen view, offscreen view, or mesh-bound view.
