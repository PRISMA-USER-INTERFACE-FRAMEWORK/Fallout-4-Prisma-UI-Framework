---
id: what-is-prismaui
title: What is PrismaUI F4?
sidebar_label: What is PrismaUI F4?
sidebar_position: 1
---

# What is PrismaUI F4?

**PrismaUI F4 is a native F4SE UI framework that lets Fallout 4 mods build interfaces with HTML, CSS, and JavaScript instead of authoring Scaleform SWFs.**

The current public framework release is **2.1.0**. It runs **Ultralight 1.4.0 in-process** and presents views through the framework's D3D11 compositor. The older legacy-runtime host/subprocess architecture is retired and is not part of the production package.

## Why it exists

Fallout 4's built-in menus use Scaleform. Editing those menus usually means working with SWF files and ActionScript. PrismaUI gives mod authors another path: build the new interface as ordinary web assets, then let an F4SE plugin control its lifecycle through a versioned C++ API.

You can still suppress selected vanilla HUD widgets or menus when replacing them, but PrismaUI does not require you to modify the original SWF files.

## How it works

1. **Build the view.** Put your HTML/CSS/JS under your mod's `Data/PrismaUI_F4/views/<PluginName>/` directory.
2. **Create it from C++.** Request a PrismaUI interface and call `CreateView("<PluginName>/index.html")`.
3. **Control its lifecycle.** Use `Show`, `Hide`, `Focus`, `Unfocus`, `Invoke`, `InteropCall`, and listener APIs as needed.
4. **Let PrismaUI own the browser/runtime boundary.** `PrismaUI_F4.dll` owns Ultralight, rendering, input routing, focus, and composition inside the Fallout 4 process.

```text
Your HTML/CSS/JS
      |
      v
PrismaUI_F4.dll
  Ultralight 1.4.0
  view lifecycle + JS bridge
  input/focus routing
  CPU BitmapSurface presentation
      |
      v
Fallout 4 frame
```

There is no `retired host component`, previous browser runtime subprocess, shared legacy-runtime shell, or `retired runtime library` in 2.1.0.

## Supported desktop runtimes

PrismaUI_F4 2.1.0 supports:

- Fallout 4 **1.10.163** (OG)
- Fallout 4 **1.11.137 and later** in the AE family when matching Address Library data is available

The intermediate **1.10.980-1.10.984 Next-Gen line is deliberately rejected** by the framework. Do not advertise it as a supported runtime.

Fallout 4 VR uses a separate provider DLL and separate validation/release path. It is not part of the public desktop 2.1.0 package.

## What you get

### Versioned C++ API

The public header exposes `IVPrismaUI1` through `IVPrismaUI10`. Request the lowest interface that has the methods you need.

```cpp
auto* api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI10>();
if (!api) {
    return;
}
```

The current header also exposes capability discovery. For optional V10 input-region behavior, check the capability before depending on it:

```cpp
if (PRISMA_UI_API::HasPrismaCapability(PRISMA_UI_API::PrismaCapability::InputRegions)) {
    // FocusOverlay / SetInputRegions are available.
}
```

### C++ and JavaScript bridge

Push data into the view with `InteropCall` or `Invoke`, and receive events with `RegisterJSListener` or `BindUIEvent`.

```cpp
g_api->InteropCall(view, "updateHealth", jsonPayload);

g_api->RegisterJSListener(view, "onClose", [](const char*) {
    g_api->Hide(g_view);
});
```

### Focus and panel coordination

The API includes focus management, view roles, focused-view lookup, panel visibility checks, and V10 input-region support. This lets multiple PrismaUI mods coordinate instead of blindly opening interactive panels on top of one another.

### Fallout integration

The framework provides APIs for translations, controller prompts, selected vanilla UI suppression, view health, offscreen rendering, and other Fallout-specific UI tasks. Some calls have runtime-specific limits, so read the method documentation instead of assuming every feature is identical across OG and AE.

## Web-platform expectations in 2.1.0

Ultralight is not previous browser runtime. Do not rely on old documentation that promised the complete previous browser runtime/Electron feature set.

For 2.1.0:

- Standard HTML/CSS/JavaScript is the intended authoring model.
- React, Vue, Svelte, and similar frameworks can be used when compiled/bundled into compatible static assets.
- Self-host your JavaScript, styles, fonts, and other required dependencies with the mod.
- Worker-style browser APIs are blocked by the Prisma sandbox.
- Remote networking from the page is not a general-purpose plugin networking interface. Put required HTTP/WebSocket work in C++ and send results to the view through the Prisma bridge.
- Arbitrary `file://` access is not a supported content path.
- The 2.1.0 shipping renderer is the CPU BitmapSurface path. Accelerated Ultralight rendering remains internal/deferred.

See [Networking](networking) and [Limitations](limitations).

## What PrismaUI is not

- **Not a Scaleform editor.** It renders a separate UI layer and can optionally suppress selected vanilla UI at runtime.
- **Not previous browser runtime/legacy-runtime in 2.1.0.** legacy-runtime is retired from production.
- **Not unrestricted browser hosting.** The framework deliberately confines local assets and network behavior.
- **Not a replacement for your native plugin logic.** Game access, networking, and engine-heavy work belong in C++; PrismaUI handles the presentation and bridge.

## Start here

[Getting Started](getting-started) walks through creating and deploying a consumer plugin. The [API Reference](api-reference) documents the current interface surface, and the [current API header](https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Fallout-4-Prisma-UI-Framework/blob/main/src/PrismaUI_F4_API.h) is the exact header mod authors should compile against.
