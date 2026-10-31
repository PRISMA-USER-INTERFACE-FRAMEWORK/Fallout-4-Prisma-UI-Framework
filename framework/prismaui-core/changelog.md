# PrismaUI_F4 Changelog

## 2.1.0.1

### Controller ABI and native input

- Failed native menu-converter installation now disables and removes every created MinHook, including the case where create succeeds and a later enable fails.
- PrismaUI runtime eligibility remains OG `1.10.163` and AE `1.11.137+`; the native controller adapter is deliberately narrower, with hash-bound static converter/sender evidence for `1.10.163` and `1.11.240` only. The direct `ScaleformMenuEvent::key` consumer proof and live controller acceptance remain open, while unsupported/intermediate runtimes fail closed. FO4VR controller/runtime support is deferred to the separate VR plugin work tracked under #740, #741, and #742.
- `BindControllerFocusEntry` now keeps one registration per `(view, button)` instead of one per view. A panel can register several unfocused entry buttons; rebinding or unbinding one no longer drops the others. The V12 signatures and slots are unchanged.
- Native menu conversion and press/release pairing remain unique to this controller runtime. Frozen V9-V12 epoch tokens stay `0x88`-`0x8B`. V12 is not retired. Native Gamepad control is on the modern Controller table.

### Graphics compatibility

- Unfamiliar graphics ownership no longer disables Prisma by itself. When both presentation targets are readable and pass Prisma's hook classifier, Prisma safely chains through the swapchain instead of overwriting foreign code.
- Targets that cannot be safely classified or chained still fail soft: Prisma gives up only its own overlay coverage and leaves the other renderer untouched.
- Hook diagnostics report the owner, classifier result, and whether Prisma selected direct hooking, safe chaining, or rejection.

## PrismaUI_F4 2.1.0

PrismaUI_F4 2.1.0 is a major framework rewrite and the largest architectural change since the 2.0 line. The framework has returned to **Ultralight 1.4.0** as its active HTML/CSS/JavaScript runtime.

This was not a simple renderer swap. The browser runtime, view lifecycle, JavaScript bridge, input/focus ownership, frame handoff, graphics-hook safety, ModelPreview continuity, local-map integration, network sandbox, diagnostics, packaging, and release pipeline were all reworked around the new backend.

Version numbering has also been realigned with the public Nexus release series. Earlier 2.2.x GitHub-only builds used an internal numbering scheme and are superseded by the public 2.1.0 line.

## Why PrismaUI returned to Ultralight

PrismaUI 2.0 moved to an out-of-process Chromium-based browser architecture because it offered a very broad web platform. In practice, that architecture was a poor fit for an injected Fallout 4 framework.

The external browser host and subprocess model added several layers that Prisma had to own and recover correctly: process startup and shutdown, IPC, browser-process lifetime, renderer-process lifetime, transport state, shared textures, frame synchronization, crash recovery, and interaction with Fallout's own D3D11 presentation path. Every additional boundary created another place where a menu could go blank, a browser process could stop responding, shutdown could race, or a graphics wrapper could change the environment underneath Prisma.

It also carried substantially more runtime weight than Prisma needs for local game UI. Prisma interfaces are packaged local HTML/CSS/JavaScript applications, not general-purpose web browsers. Running a large multiprocess browser stack for that workload created unnecessary process, memory, IPC, synchronization, deployment, and diagnostic overhead.

Ultralight is a better fit for the framework's actual job:

- It runs directly inside PrismaUI instead of requiring a separate browser host and subprocess tree.
- View creation, JavaScript execution, rendering, callbacks, and destruction can be serialized through one managed owner thread.
- C++ to JavaScript calls no longer need to cross an external-process transport boundary.
- The runtime package is smaller and contains fewer moving parts.
- CPU-rendered BitmapSurfaces give Prisma explicit ownership of the browser pixels before they are uploaded to Fallout's D3D11 renderer.
- Failure handling is simpler because the framework owns the complete view lifecycle in one process.
- The architecture is easier to reason about around save/load, shutdown, graphics wrappers, Frame Generation, ENB, and other components that also touch Fallout's renderer.

The result is a leaner and more predictable runtime with lower framework overhead and fewer synchronization boundaries. The goal of 2.1.0 is not to chase browser feature count; it is to provide a fast, stable UI runtime designed specifically for Fallout 4 mods.

## Runtime and rendering rewrite

- Replaced the previous browser provider with an in-process **Ultralight 1.4.0** backend.
- Rebuilt view creation, loading, DOM-ready handling, JavaScript listeners, console callbacks, failure states, destruction, and shutdown around a dedicated Ultralight owner thread.
- Rebuilt Ultralight GPU acceleration around Prisma's D3D11 integration, including explicit device/context ownership validation, transactional resize/device recovery, bounded generation transport, and safe compositor handoff.
- Accelerated generations execute through Fallout's validated D3D11 rendering context instead of the earlier deferred-context design.
- CPU BitmapSurface rendering remains available as a controlled fallback when GPU activation is not eligible or the graphics environment cannot be safely claimed.
- Removed the external browser-process transport boundary from normal `Invoke`, `InteropCall`, listener, and lifecycle operations.
- Added bounded pending-call queues and replay so DOM-ready transitions cannot flood the browser owner thread.
- Added strict runtime staging and validation for the shipped Ultralight runtime and resources.

## Performance and memory

The move back to Ultralight removes the cost and complexity of maintaining a separate browser host/process tree and the IPC synchronization required to move state and frames between processes.

Additional 2.1.0 performance work includes:

- Reduced redundant browser and compositor work when content has not changed.
- Bounded presentation waits so Fallout is never expected to wait indefinitely for a Prisma frame.
- Bounded ModelPreview scheduling and resource residency.
- Bounded JavaScript pending-call count and memory use.
- Controlled Ultralight/JSC memory configuration instead of allowing browser heaps and caches to scale freely with system memory.
- Reclamation of hidden-view rendering buffers under memory pressure with lossless regeneration when shown again.
- The accelerated path removes the external browser texture/IPC ownership layer, while the CPU fallback remains directly controlled by Prisma when needed.

## Stability and Fallout integration

- Reworked save/load protection so browser, ModelPreview, and engine-resource work does not race Fallout while the world is being rebuilt.
- Added stronger view terminal-state handling so failed or destroyed views cannot later resurrect from delayed callbacks.
- Improved focus, pause, and input recovery when a view fails, disappears, or becomes unresponsive.
- Added a Present-independent recovery watchdog so input can be returned to Fallout even when the normal presentation path stops firing.
- Improved frame continuity so Prisma can retain the last complete valid frame during temporary producer loss without committing incomplete content.
- Improved ModelPreview and TextureOverlay continuity so temporary resource eviction is not confused with explicit Hide/Destroy.
- Expanded freeze/crash diagnostics with browser-owner state, D3D11 breadcrumbs, and optional freeze dumps.

## Graphics-hook coexistence

2.1.0 substantially tightens Prisma's D3D11 hook policy.

- Prisma verifies ownership before installing, moving, disabling, or removing Present/ResizeBuffers hooks.
- Unknown or already-modified graphics entry points fail closed instead of being overwritten.
- Hook migration is transactional so a partial migration cannot leave Present and ResizeBuffers in different ownership states.
- If another injector takes ownership after Prisma, the old Prisma hook is quarantined rather than restoring bytes over the newer hook.
- ENB and ENB plus Frame Generation received explicit runtime validation during development.
- Other graphics/injector combinations remain outside the tested support promise until individually validated.

This is a deliberate change in philosophy: Prisma prefers to lose its own flat-overlay coverage rather than damage or corrupt another renderer's hook chain.

## API and developer compatibility

- Existing public API **V1 through V10 remains ABI-compatible**.
- API V11 adds verified game-thread dispatch helpers including `DispatchToGameThread`, `IsGameThread`, and `BindGameThreadUIEvent`.
- JavaScript callbacks and registered listeners are marshalled through the framework's managed lifecycle instead of an external browser transport.
- The public developer headers, Papyrus source/binary, documentation, examples, MCP tooling, and website were synchronized to the released 2.1.0 contract.

## Security and local-content model

PrismaUI remains a local game-interface framework rather than a general web browser.

- Local documents are resolved through a contained path policy.
- Local HTML receives a generated Content Security Policy.
- Remote top-level navigation is rejected at the view/load boundary.
- Native listeners are not installed into untrusted remote documents.
- Network behavior is covered by dedicated acceptance/static tests for scripts, styles, fonts, frames, media, XHR/fetch, WebSockets, workers, forms, and redirects.

## Runtime support

Standard Fallout 4 package:

- Fallout 4 Old Gen: **1.10.163**
- Fallout 4 Anniversary Edition: **1.11.137+** where the required Address Library/runtime data exists

The intermediate Fallout 4 1.10.980-1.10.984 Next-Gen line is not part of the current 2.1.0 runtime support policy.

Fallout 4 VR remains a separate build/runtime path.

## Packaging and release engineering

- Release packaging now stages the exact Ultralight runtime and required legal/notices payload.
- Release packages verify required binaries, runtime resources, API exports, configuration, and checksums before publication.
- The 2.1.0 release process retains exact DLL/ZIP provenance so published bytes can be traced back to the prepared release candidate.
- Debug symbols are retained separately for native crash analysis.
- `bMeshBinding=0` remains the deterministic packaged default while the engine-object redesign is deferred.

---

## v1.8

<details>
<summary><b>Click to expand Version 1.8 Changelog</b></summary>

### Stability & Memory
- Tuned the Ultralight engine configuration to dramatically cut per-view memory usage. JavaScriptCore now sizes its heaps conservatively instead of scaling to full system RAM (override_ram_size capped at 1 GB), the initial JS heaps were shrunk (large 32 MB → 8 MB, small 1 MB → 512 KB), the WebCore resource cache was reduced (64 MB → 32 MB), the back/forward page cache was disabled, and the renderer thread count was capped at 2.
- Resolved a class of out-of-memory crashes in heavy load orders where the framework's memory footprint, alongside many active views, starved the game's Scaleform UI heap (Scaleform allocation failures leading to a CTD).

### Reliability Watchdog
- Added a health monitor for every hosted plugin view. Every 30 seconds it logs a summary (view count, visible/hidden/quarantined, faulting views, process working set) plus a detail line per problem view, so a misbehaving plugin is easy to identify from the log.
- Views that repeatedly fail crash-recovery are now quarantined — isolated from the renderer so a single broken view can no longer destabilize the framework or the game. The view handle stays valid, so the owning plugin keeps working and can recreate it.
- Under high process-memory pressure, the render buffers (CPU pixel buffer and GPU texture) of hidden views are reclaimed automatically and regenerated losslessly when the view is shown again.

### Input & Interaction
- Added click-through for transparent regions of a focused view: a click over an empty (transparent) area now passes to the game — for example to interact with a 3D model rendered behind the interface — while a drag that began over real UI content stays captured until release.
- Fixed mouse input capture by retaining the focus menu's modal flag; without it, the menu beneath could steal mouse input through the game's device layer.

</details>

---

## v1.7

<details>
<summary><b>Click to expand Version 1.7 Changelog</b></summary>

### Framework Improvements
- Added new NotificationSystem for displaying in-game notifications.
- Enhanced InputHandler functionality and reliability.
- Improved ViewManager architecture and view lifecycle handling.
- Updated communication layer between C++, JavaScript, and game systems.
- Finalized transition to xmake as the sole build system.
- Removed legacy CMake support.

### Build & Deployment
- Consolidated build.bat, deploy.bat, setup.bat, and verify-deploy.bat into a single build-and-deploy.bat script.
- Added automatic GitHub version checking for SDK dependencies.
- Added automatic Ultralight SDK extraction during setup/build.
- Improved deployment error handling and validation.
- Finalized xmake project configuration.

### Example Plugin Improvements
- Fixed Event Log copy functionality by sending the full log through the C++ bridge instead of relying on the browser Clipboard API.
- Fixed Copy button state reset issues.
- Added C++ → JavaScript callback support for sendDataToF4SE.
- Event Log now displays framework callback messages.
- Enabled scrolling for Event Log container via overflow-y: auto.
- Improved optional API guards and error reporting.
- Added additional logging throughout the example plugin.

### Papyrus Bridge & Property System
- Updated FormID resolution to use TESDataHandler for plugin-aware lookups.
- Property reads now correctly account for plugin load order offsets.
- Property writes now correctly account for plugin load order offsets.
- Removed unsupported property write functionality from the example plugin.
- Clarified Papyrus Bridge limitations and supported workflows.

### Documentation
- Added a comprehensive Tutorial tab featuring 9 guided sections covering framework basics, view creation, communication APIs, event handling, Papyrus integration, notifications, common patterns, best practices, and troubleshooting.
- Added:
  - CHANGELOG.md
  - limitations.md
  - papyrus-bridge.md
  - translations.md
  - modern-frameworks.md
- Expanded documentation for:
  - View lifecycle behavior
  - HTML view support
  - JavaScript support matrix
  - API reference examples

### New Assets
- Added notification-banner.html component.
- Added new example and reference documentation assets.

### Refactoring
- Cleaned up API comments throughout the framework.
- Improved code organization and maintainability.
- General documentation and comment consistency improvements.

### Breaking Changes
- Removed CMake support.
- xmake is now the only supported build system.

</details>

---

## Version 1.6

<details>
<summary><b>Click to expand Version 1.6 Changelog</b></summary>

### Papyrus Bridge — `window.prisma`

<details>
<summary><b>💡 What is the Papyrus Bridge? (Click to expand)</b></summary>

Up until now, creating an interface with PrismaUI required writing a dedicated C++ plugin, managing view lifetimes, and handling multi-threading. The Papyrus Bridge completely removes that barrier for Papyrus modders.

PrismaUI now automatically injects a `window.prisma` object into every single HTML view out of the box. No C++ required, no compilers, and no extra configuration. You can now read and write live game states directly from JavaScript.

**The Workflow:**
1. Declare your Auto properties on a Quest script like you normally would.
2. Place your HTML/JS assets inside `Data/PrismaUI_F4/views/`.
3. Call `prisma.getProperty` or `prisma.setProperty` directly from your frontend code.

**Quick JavaScript Examples:**

```js
// Read and write a live Papyrus Auto property on a quest script
const damage = await prisma.getProperty("MyMod.esp", "800", "MyMod_Quest", "DamageScale");
prisma.setProperty("MyMod.esp", "800", "MyMod_Quest", "DamageScale", 2.5);

// Read and write a TESGlobal value
const val = await prisma.getGlobal("MyMod.esp", "801");
prisma.setGlobal("MyMod.esp", "801", 3.0);
```

#### Best Practices & Limitations:

- **Pull-Based Architecture:** The system is currently pull-based. If Papyrus updates a property internally, the UI won't update automatically. For standard config menus, simply poll the values using a standard JavaScript `setInterval`. A push/subscribe architecture is planned for a future release.
- **Supported Data Types:** Version 1 matches `float`, `int`, and `bool` properties. Strings and arrays are not yet supported.
- **Host Selection:** Always host your target script properties on a persistent **Quest form** rather than a cell-local reference. If the script host unloads from game memory, the bridge will safely return `null` and log a warning to prevent crashes.
- **VM Availability:** Ensure reads are executed after the Papyrus VM is fully initialized (`kPostLoadGame`). Menus invoked mid-game naturally bypass this constraint.

**New Feature Highlights:**

- Implemented the "Direct Papyrus VM Integration": every HTML view now has `window.prisma` injected automatically before `OnDomReadyCallback` fires, removing the need for C++ wrappers in consumer plugins.
- Added `getGlobal(esp, formId)` and `setGlobal(esp, formId, value)` to read/write `TESGlobal` float values.
- Added `getProperty(esp, formId, scriptName, propName)` and `setProperty(...)` to read/write Papyrus Auto properties (float, int, bool) via handle-scoped VM lookup.
- Architecture: promise-based async reads; writes are fire-and-forget. All reads return `null` on failure (e.g., missing plugin, wrong form, VM not ready) and will not throw errors.
- Threading: JavaScript handlers execute on the Ultralight thread, while game data access is safely dispatched via `AddTask`.
- Documentation: Updated `api-reference.md` and `getting-started.md` to reflect the new integration.

</details>

### Example Plugin — Full API Guide HTML

- Rewrote `example-f4se-plugin/view/index.html` from a basic demo into a 3-tab interactive guide:
  - **Papyrus Bridge tab:** Live `getGlobal`/`setGlobal`/`getProperty`/`setProperty` testing with inputs, result display, and null-return references.
  - **C++ Bridge tab:** Documents `Invoke`, `InteropCall`, `BindUIEvent`, and `RegisterJSListener` with a live focus badge demo and send-to-plugin input.
  - **Event Log tab:** Features a unified, color-coded bridge activity log and a `console.log` test button.


### Migration to NewCommonLib (xmake)

Both the example plugin and the core `PrismaUI_F4.dll` have migrated from `cmake`/`CommonLibF4_OG` to the new xmake-based `NewCommonLib`.

#### Example Plugin (`example-f4se-plugin`)

- `xmake.lua`: includes `../NewCommonLib`, implements the `commonlibf4.plugin` rule, and removes dependencies on `boost`, `mmio`, and `fmt`.
- `PCH.h`: removed `spdlog` aliases; logging is now handled via `REX::CRITICAL`/`INFO`.
- `main.cpp`: uses `F4SE_PLUGIN_LOAD` and `F4SE::Init()` to replace manual `F4SEPlugin_Query` and `spdlog` setup.
- `PrismaUI_F4_API.h`: replaced removed `F4SE::WinAPI::` calls with standard `::GetModuleHandleW` and `::GetProcAddress`.
- Fixes: pinned `spdlog` to `v1.16.0` to prevent duplicate `add_requires` errors; switched `REX::ERROR` to `REX::CRITICAL` to avoid Windows `ERROR` macro collisions.

#### Core PrismaUI_F4 DLL

- Build system: replaced `CMakeLists.txt` with a new `xmake.lua`. Pulls `minhook` and `directxtk` from the xmake registry, utilizes the existing extracted Ultralight SDK, and preserves all compiler flags from `CompilerFlags.cmake`.
- `PCH.h`: maps `namespace logger = spdlog` to replace the removed `F4SE::log`.
- `main.cpp`: replaced `F4SEPlugin_Query` and manual log setups with `F4SE_PLUGIN_LOAD` and `InitInfo{logName, trampoline}`.
- `Hooks.h/.cpp`: replaced `<dxgi.h>` SDK types with `REX::W32::IDXGISwapChain` and `DXGI_FORMAT`.
- Direct3D management: updated `Hooks.cpp`, `Core.cpp`, and `ConflictChecker.cpp` to use the `GetRendererData()` free function instead of the removed `RendererData::GetSingleton()`.
- `Core.cpp`: added `reinterpret_cast` bridges to map `REX::W32` COM types to SDK COM types for compatibility with D3D11 and DirectXTK.
- Runtime: switched the runtime library to `/MD` to align with precompiled xmake DirectXTK and `commonlibf4` packages.

</details>

---

## Version 1.5

<details>
<summary><b>Click to expand Version 1.5 Changelog</b></summary>

### Security

- **Network Sandbox:** Added an automatic network sandbox applied to all views across all API versions (V1–V4).
- **API Restrictions:** Outbound network APIs (`fetch`, `XMLHttpRequest`, `WebSocket`, `EventSource`, `Worker`, etc.) are now blocked in every view before page scripts execute and cannot be overridden.
- **`CreateView` Restrictions:** Modified `CreateView` to reject external URLs (`http://`, `https://`). The system now exclusively accepts local paths under `Data/PrismaUI_F4/views/`.
- **Navigation Restrictions:** Blocked `window.open()` and all forms of external navigation.
- **Logging:** All blocked network or navigation attempts are now logged to `PrismaUI_F4.log`.

> [!NOTE]
> There are no changes to the public API. Plugins built against V1–V4 will receive this sandbox automatically with no code changes required.

</details>

---

## Version 1.4

<details>
<summary><b>Click to expand Version 1.4 Changelog</b></summary>

### Documentation Updates

- Updated the API reference, examples, getting-started guide, and view-lifecycle documentation for V4.
- Added `IVPrismaUI4` and `BindUIEvent` documentation across the reference materials.
- Documented `RegisterTranslations` (V3) in both the API reference and examples.
- Updated all development examples to use `IVPrismaUI4` instead of `IVPrismaUI3`.
- Added an "Updating Your Plugin" migration section to `api-reference.md`.
- Updated the DOM Ready callback section to include implementation guidance comparing `BindUIEvent` vs `RegisterJSListener`.

</details>

---

## Version 1.3

<details>
<summary><b>Click to expand Version 1.3 Changelog</b></summary>

### Runtime Compatibility

- **Next-Gen F4SE Support:** Added the `g_pluginVersionData` export for F4SE 0.7.1+. Plugins now register through `PluginVersionData` alongside or in place of the legacy `F4SEPlugin_Query` path, allowing F4SE to validate the plugin prior to loading.
- **Address Library:** Added `UsesAddressLibrary(true)` support for automatic `.bin` lookup based on the game's runtime version string (e.g., `version-1-11-221-0.bin`).
- **CommonLibF4 Layouts:** Added an `IsLayoutDependent(true)` declaration for plugins utilizing `RE::` types from CommonLibF4.
- **Version Management:** An empty `CompatibleVersions` array now signals compatibility with all future runtime versions, removing the need for manual plugin updates after minor game patches.
- **Backwards Compatibility:** Legacy `F4SEPlugin_Query` and `F4SEPlugin_Load` exports remain intact; pre-0.7.1 versions of F4SE will ignore `g_pluginVersionData` and fall back to the classic loading path.
- **Validation:** Game versions below `1.10.162` are now explicitly rejected in `F4SEPlugin_Query` due to incompatible pre-Next-Gen hooks.
- **Address Bins:** The `AddressBins/` directory now ships with `.bin` files covering runtimes back to `1.10.130` out of the box.

### Core Framework

- **Rendering Engine:** Integrated an Ultralight-powered HTML/CSS/JS rendering layer into DXGI `Present` and `ResizeBuffers` using MinHook vtable patching.
- **API Exporting:** Added versioned plugin API exports (`IVPrismaUI1`, `IVPrismaUI2`, `IVPrismaUI3`) exposed via `RequestPluginAPI` using `GetProcAddress`, eliminating link-time dependencies for consumer plugins.
- **View Lifecycle:** Added complete view lifecycle management functions:
  - `CreateView` / `Destroy`
  - `Show` / `Hide`
  - `Focus` / `Unfocus`
  - `IsValid` / `SetOrder`

- **JavaScript Interop:**
  - Added `InteropCall` for high-frequency C++ to JavaScript communication.
  - Added `Invoke` for one-shot JavaScript execution with callback support.
  - Added `RegisterJSListener` to expose named window-level JavaScript functions directly to C++.

- **Console Logging:** Added `RegisterConsoleCallback` (V2) to pipe JavaScript console messages (`log`, `warn`, `error`, `debug`, `info`) directly into the C++ log.
- **Localization:** Added `RegisterTranslations` (V3), which automatically loads translation files from `Data\\Interface\\Translations\\<plugin>_<lang>.txt` and injects `window.L10N` and `window.t()` into every view on page load.
- **Debugging Tools:** Added inspector view support via `CreateInspectorView`, `SetInspectorVisibility`, and `SetInspectorBounds`.

</details>

---

## Version 1.2

<details>
<summary><b>Click to expand Version 1.2 Changelog</b></summary>

### PrismaDesigner Refactor

- **Canvas Refactor:** Overhauled canvas item handling; inserted elements are now managed through an internal reference-based system.
- **Stability:** General stability and architectural cleanup across the designer pipeline to establish foundational work for future editor and layout features.

</details>

---

## Version 1.1

<details>
<summary><b>Click to expand Version 1.1 Changelog</b></summary>

### Bug Fixes

- **Windows 10 Crashes:** Resolved crashing issues on Windows 10 by downgrading Ultralight from the `1.4.1-dev` pre-release build to the `1.4.0` stable release. The pre-release build contained a heap corruption issue in `WebCore.dll` that caused random crashes within minutes across unrelated DLLs (such as `usvfs`, `ConsoleAutocomplete`, and `Qt6Network`). Windows 11 systems were unaffected.
- **Hook Conflicts:** Fixed a hook conflict caused by enabling all MinHook hooks globally (`MH_ALL_HOOKS`) during startup. PrismaUI now selectively enables only its own Direct3D hooks (`Present` and `ResizeBuffers`).

### Improvements

- **Render Loop:** Added the required `RefreshDisplay()` call to the render loop per Ultralight `1.4.0` specifications. CSS animations, scrolling, and `requestAnimationFrame()` now function correctly.

### PrismaDesigner Updates

- **Typography:** Added the *Overseer* Fallout-style font to the text element dropdown. Exported HTML automatically embeds this font as base64 so that exported views are entirely self-contained.
- **Heading Element:** Added a new Heading element to the palette. Headings default to the *Overseer* font, a 48px font size, and an Amber color palette, allowing titles to be dropped directly into layouts.

> [!TIP]
> If you experienced crashes on Windows 10, update `PrismaUI_F4` and replace the binaries inside `PrismaUI_F4\libs\` with the DLLs included in this release.

</details>
