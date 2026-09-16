# PrismaUI_F4 Changelog

Release history for the Fallout 4 Prisma UI Framework. Newest first.

---

# PrismaUI_F4 2.1.0

**Release date:** 2026-08-31

PrismaUI_F4 2.1.0 is the largest framework rewrite since the 2.0 line. The active HTML/CSS/JavaScript runtime has returned to **Ultralight 1.4.0**, running directly inside PrismaUI_F4.

This was not a simple browser-library swap. The runtime boundary, view lifecycle, JavaScript bridge, input and focus ownership, frame handoff, D3D11 rendering path, graphics-hook safety, ModelPreview continuity, local-map integration, network sandbox, diagnostics, packaging, and developer tooling were all reworked around the new architecture.

Version numbering is aligned with the public 2.1.0 release series. Earlier GitHub-only 2.2.x numbers were internal development numbering and are superseded.

## Why PrismaUI returned to Ultralight

PrismaUI 2.0 moved to CEF because Chromium offered an extremely broad browser feature set. In practice, the multiprocess browser architecture was a poor fit for an injected Fallout 4 framework.

The CEF design required Prisma to manage substantially more machinery than a local game UI framework actually needs:

- external browser-host startup and shutdown
- subprocess lifetime and crash handling
- IPC between Fallout and the browser process
- shared frame and texture transport
- browser-process and renderer-process synchronization
- shutdown ordering across multiple processes
- browser recovery independent of Fallout recovery
- D3D11 frame handoff across process boundaries
- graphics-wrapper and injector interaction on the Fallout side

Those extra boundaries increased memory use, startup/shutdown complexity, deployment size, synchronization cost, and the number of failure states Prisma had to recover from.

A UI could be perfectly valid while the framework still had to deal with a stalled subprocess, failed frame handoff, browser transport loss, shutdown race, stale shared texture, or graphics wrapper changing the presentation environment underneath Prisma.

Prisma interfaces are packaged local HTML/CSS/JavaScript applications. They do not need a full desktop-browser process tree.

Ultralight is a better fit for the job:

- It runs **inside PrismaUI_F4** instead of requiring a separate browser process tree.
- View creation, JavaScript execution, rendering, callbacks, and destruction are controlled by one managed owner thread.
- C++ to JavaScript calls do not cross an external-process transport boundary.
- The runtime package is smaller and has fewer moving parts.
- Prisma can directly control browser-frame ownership before Fallout composition.
- Save/load, focus recovery, shutdown, ENB, Frame Generation, and graphics-hook coexistence can be handled from one process.
- Failure recovery is substantially easier to reason about because the full view lifecycle belongs to Prisma.

The result is a leaner and more predictable framework with lower overhead and fewer synchronization boundaries.

The goal of 2.1.0 is not to provide every capability of a desktop browser. The goal is a fast, stable, purpose-built UI runtime for Fallout 4 mods.

## How large is the rewrite?

Almost every framework subsystem was touched or replaced:

- runtime startup and ownership
- view creation and destruction
- DOM-ready handling
- JavaScript evaluation
- JavaScript listeners and callbacks
- input ownership
- focus and pause recovery
- controller routing
- browser-frame transport
- D3D11 composition
- accelerated GPU rendering
- device and swapchain lifecycle
- Present and ResizeBuffers hook ownership
- ModelPreview
- TextureOverlay
- local-map presentation
- held-frame continuity
- save/load protection
- runtime security
- network policy
- freeze/crash diagnostics
- release packaging
- symbol retention
- API/header validation
- Papyrus bridge packaging
- public documentation and MCP tooling

This is a framework-level rewrite, not a cosmetic backend change.

---

## Highlights

- Returned the active browser runtime to **Ultralight 1.4.0**.
- Removed the production dependency on the CEF Host/subprocess architecture.
- Rebuilt Ultralight rendering around a controlled in-process lifecycle.
- Added the rewritten **Ultralight GPU-accelerated D3D11 path**.
- Kept CPU BitmapSurface rendering available as a controlled fallback when GPU activation is not safe or eligible.
- Reworked D3D11 device, context, swapchain, and hook ownership validation.
- Added transactional device/ResizeBuffers recovery.
- Added bounded accelerated-generation transport.
- Reworked save/load safety for browser, ModelPreview, TextureOverlay, and engine-resource access.
- Improved focus, pause, controller, and input recovery.
- Reworked held-frame continuity so incomplete browser frames cannot replace a valid frame.
- Improved ModelPreview lifetime, scheduling, resource ownership, and stale-result rejection.
- Added stronger graphics-hook coexistence with ENB, Frame Generation, and other D3D11 interposers.
- Expanded freeze/crash diagnostics and D3D11 breadcrumbs.
- Hardened local-content and network security.
- Preserved the public numbered API through **IVPrismaUI10**.
- Synchronized the public C++ header, Papyrus source/binary, examples, docs, website, and MCP tooling to 2.1.0.

---

## Ultralight runtime architecture

### In-process runtime

- Ultralight 1.4.0 now runs directly inside `PrismaUI_F4.dll`.
- Each Prisma interface is represented by a managed Ultralight View.
- View creation and destruction are owned by the framework rather than an external browser host.
- JavaScript execution and native callbacks stay inside the same framework process.
- Browser ownership is serialized through a dedicated owner thread.
- Runtime shutdown has explicit stop state and bounded diagnostics.
- The Ultralight runtime remains resident for the Fallout process lifetime where unloading during late process teardown would be unsafe.

### Runtime package validation

- Required Ultralight runtime files are staged as part of the release package.
- Runtime resources are validated before browser startup.
- Required package files, API exports, configuration, and release provenance are checked during release construction.
- AppCore/WebCore remain normal shared-library components rather than being hard-locked at installed runtime.
- Legal notices and source/attribution material required by the bundled runtime components are packaged with the framework.

### Session behavior

- Prisma uses a non-persistent browser session.
- Temporary browser/session state is not silently retained across Fallout sessions.
- Prisma interfaces continue to use local content under the framework's contained views directory.

---

## GPU acceleration and rendering

The Ultralight renderer was substantially redesigned during the 2.1.0 development cycle.

### Rewritten accelerated path

- Ultralight GPU acceleration now integrates through Prisma's validated Fallout D3D11 path.
- The earlier custom deferred-context design was retired after proving unreliable with real Fallout graphics stacks and wrappers.
- Accelerated generations execute through the validated Fallout D3D11 rendering context.
- Device, context, swapchain, and hook provenance are checked before accelerated execution is accepted.
- Unknown or inconsistent ownership fails closed.
- Accelerated generation transport is bounded so browser work cannot grow without limit.
- Resource-only generations are drained correctly even when they do not immediately produce a visible draw.
- Texture retirement is deferred while dependent render buffers are still alive.
- Stale resources are invalidated across device and swapchain epochs.
- CPU-side payloads are retained where required so GPU resources can be rebuilt after a validated lifecycle change.

### Device and ResizeBuffers lifecycle

- Device/context/swapchain state is tracked as a render lifecycle epoch.
- ResizeBuffers transitions invalidate stale accelerated resources transactionally.
- Device removal/reset failures fail closed instead of continuing with invalid objects.
- Published browser frames from an old device generation are not reused on a new device generation.
- Held copies and present leases are invalidated when ownership changes.

### CPU fallback

- CPU BitmapSurface rendering remains available as a controlled fallback.
- CPU browser pixels are owned by Prisma before upload to Fallout's D3D11 renderer.
- The fallback path is used when the graphics environment cannot safely enter the accelerated path.
- CPU fallback is no longer the entire rendering story for 2.1.0; it is one controlled path inside the new renderer architecture.

---

## Performance and memory

Moving back to an in-process browser runtime removes a large amount of process and synchronization overhead from the framework architecture.

### Browser/runtime overhead

- No external browser-host process tree is required for normal Prisma views.
- No external-process JavaScript bridge is required for normal `Invoke`, `InteropCall`, or listener traffic.
- No separate browser-process lifetime has to be synchronized with Fallout shutdown.
- No external browser frame transport layer is required before Prisma can reason about frame ownership.

### Frame and compositor work

- Presentation waits are bounded so Fallout is not expected to wait indefinitely for a Prisma frame.
- Incomplete browser frames do not overwrite a previously complete committed frame.
- Redundant compositor work is reduced when content has not changed.
- Known source dimensions are reused where possible instead of repeatedly querying D3D metadata.
- Overlay ordering and visibility work are shared rather than repeatedly rescanning the same state.

### Memory controls

- JavaScript pending-call count is bounded.
- Pending JavaScript memory use is bounded.
- High-frequency status work can be coalesced while a view is loading.
- ModelPreview work queues are bounded.
- ModelPreview resident resources are capped.
- Hidden-view browser buffers can be reclaimed under memory pressure and regenerated when shown again.
- Browser/JSC memory configuration is controlled rather than allowing browser caches and heaps to scale freely with total system RAM.

---

## Graphics-hook coexistence

PrismaUI 2.1.0 substantially tightens how it interacts with Fallout's D3D11 presentation path.

### Hook ownership

- Present and ResizeBuffers targets are validated before Prisma modifies them.
- Readability and module ownership are checked before hook installation.
- Already-modified or unknown hook targets fail closed.
- Prisma does not treat a foreign inline hook as permission to overwrite another renderer.
- Hook eligibility is revalidated immediately before mutation.
- Post-enable ownership is checked before the hook is published as active.

### Transactional hook migration

- Present and ResizeBuffers migration is treated as one transaction.
- Both replacement hooks must be prepared successfully before the new pair becomes active.
- A partial failure rolls back rather than leaving half of the graphics chain migrated.
- If another injector takes ownership after Prisma, the old Prisma hook is quarantined rather than restoring bytes over the newer owner.
- Quarantined hook slots retain the trampoline needed to continue the existing hook chain.

### ENB and Frame Generation

- Controlled compatibility work was completed for ENB-owned D3D11 proxy chains.
- Frame Generation proxy ownership is classified rather than blindly trusted.
- Device/context provenance must agree before accelerated execution is accepted.
- ENB + Frame Generation combinations can enter the accelerated path only when the complete ownership chain is validated.
- Prisma prefers to lose its own flat-overlay coverage rather than corrupt another renderer's hook chain.

---

## View lifecycle

The old browser lifecycle assumptions were replaced with an explicitly managed Ultralight lifecycle.

- View IDs are reserved before asynchronous creation completes.
- Creation failure is terminal for that creation attempt.
- A failed view cannot later become healthy only because a delayed callback arrived.
- Destroy requests made while creation is still in progress are honored.
- Destroyed views cannot be resurrected by late work.
- Failed/destroyed views reject stale queued operations.
- Views cannot take focus before they are ready.
- Show/order operations cannot recreate stale state after destruction.
- Browser listener ownership is detached before the underlying view is destroyed.
- DOM-ready work is queued and replayed through the managed owner.
- Pending work is replayed in bounded batches rather than monopolizing the owner thread.

---

## JavaScript and native UI events

- JavaScript evaluation runs through the managed Ultralight owner.
- `Invoke` remains available for one-shot JavaScript evaluation.
- `InteropCall` remains available for high-frequency native-to-JavaScript calls.
- Registered JavaScript listeners are installed into each view's JavaScript context.
- Calls submitted before DOM-ready can be queued and replayed after the page is ready.
- Pending-call count and total memory are bounded.
- High-frequency ModelPreview/TextureOverlay status work can be coalesced while loading.
- Console callback severity mapping was corrected for Ultralight's Log, Warning, Error, Debug, and Info ordering.
- Native callbacks are routed through the framework's managed lifecycle rather than an external browser transport.

---

## Input, focus, controller, and pause recovery

- Focus is refused for views that are still creating or have failed.
- Focus ownership, input ownership, and pause ownership are tracked independently.
- A failed focused view can automatically release Fallout input and pause state.
- A hidden or destroyed focused view cannot permanently trap input.
- A Present-independent watchdog can return control to Fallout even if the normal presentation hook stops firing.
- Minimized Fallout windows are not automatically treated as dead renderers.
- Recovery state is tracked per focus episode.
- Input installation is independent of the flat Present hook.
- Keyboard character events use Ultralight's native character-event path.
- Enter/newline handling was corrected.
- Controller handled-state propagation was tightened so mapped UI controls do not leak through to Fallout underneath.
- Unmapped controls can continue to Fallout when Prisma does not need them.

---

## Public API compatibility

The public developer ABI remains intentionally stable.

### V1-V10 compatibility

- Existing numbered interfaces remain available through `IVPrismaUI10`.
- Existing plugins using earlier API versions continue to request the version they were built against.
- V10 includes view roles, panel coordination, focused-view discovery, `FocusOverlay`, and `SetInputRegions`.
- Capability discovery exposes support such as input regions without forcing callers to guess based on framework version strings.

### Compatibility methods

Some very old API methods remain present for ABI/source compatibility even where the current Ultralight backend does not provide the old behavior.

- Inspector compatibility methods remain in the interface but do not provide the old embedded debugging model.
- Scrolling-pixel-size compatibility methods remain no-op/unsupported behavior.
- `EnableActivateChoiceFilter` retains the historical `dropDefaultTake` parameter for ABI compatibility, but default-take filtering is not implemented.
- `SuppressActivateChoicePerk` remains a compatibility placeholder.

---

## Panel coordination and input regions

- Views can identify their role to the framework.
- Consumers can query the currently focused Prisma view.
- Consumers can determine whether another Prisma panel is visible.
- `FocusOverlay` allows overlays to take focused interaction without treating every transparent region as active UI.
- `SetInputRegions` lets a view provide explicit interactive rectangles.
- Transparent regions can pass clicks back to Fallout.
- A drag that starts over active UI remains captured until release so gestures are not split between Prisma and the game.

---

## Papyrus bridge

The current public framework continues to expose the native `window.prisma` bridge for supported Papyrus workflows.

- Per-view plugin ownership is enforced.
- Property/global reads are scoped to the appropriate plugin ownership rules.
- Vanilla-master reads follow the bridge's permitted access model.
- Async read failures can reject rather than silently pretending every failure is a valid value.
- Supported property writes remain number/bool oriented.
- Writes are fire-and-forget.
- `prisma.emit()` is available for bridge event emission.
- Form resolution uses plugin-aware Fallout form lookup.
- Public `PrismaUI.psc` and `PrismaUI.pex` are kept synchronized with the released framework package.

---

## Translations

- Fallout translation files remain supported.
- Translation registration is applied after the view has a live document/DOM-ready state.
- `window.L10N` and `window.t()` are injected into the running view through the framework bridge.
- Translation work is no longer documented as if it were guaranteed before page scripts execute.

---

## ModelPreview

ModelPreview received substantial lifetime, scheduling, and rendering work.

- Preview work is budgeted rather than allowed to compete freely with the rest of the UI.
- Jobs cannot apply stale results after the selected item has changed.
- Queued work can be cancelled when a preview/view is hidden or destroyed.
- Newer queued requests for the same preview can replace older pending requests.
- Preview entries use stable continuity identities.
- Temporary resource eviction is distinguished from an explicit Hide/Destroy.
- Explicit removal immediately invalidates held content for that preview.
- Resident-limit eviction does not incorrectly mark the preview as permanently removed.
- Preview render targets are created as complete units before publication.
- Preview worker shutdown is drained safely.
- Preview textures can be requested through safer Fallout-thread scheduling rather than unsafe render-thread resource access.
- Removed previews cannot remain permanently baked into a held composite frame.

### JavaScript bridge

The current bridge is exposed as:

```js
window.__prismaUI_modelPreview
```

with `show`, `hide`, and `onStatus` support matching the released ModelPreview bridge contract.

---

## TextureOverlay and local map

- TextureOverlay instances use per-show continuity identities.
- Hide followed by Show cannot be confused with an older held instance.
- Overlay removal invalidates committed composites that still contain the removed content.
- Local-map and ModelPreview overlays can independently use live or held content.
- One missing overlay does not force every other overlay into the same held/live state.
- Local-map version-specific engine functions fail closed when the exact runtime authority is unavailable.

---

## Save/load safety

Fallout's save/load transitions rebuild large amounts of game state while Prisma still has browser and rendering work queued.

2.1.0 adds explicit protection around that boundary:

- Browser and engine-resource work can be suspended during unsafe load phases.
- ModelPreview does not perform unsafe Fallout resource reads while the world is rebuilding.
- Load grace periods prevent normal loading transitions from being mistaken for framework freezes.
- Stale work from the pre-load world cannot silently publish into the post-load world.
- Focus/input recovery remains available if a view disappears during load.

---

## Frame continuity

- Prisma retains the last complete valid browser frame when the producer temporarily misses a frame.
- Incomplete frames are not committed over valid content.
- Held web/overlay textures are copied into Prisma-owned resources.
- Held-frame capture is transactional.
- Failed recapture leaves the previous valid frame unchanged.
- Removing an overlay invalidates a held composite that still contains it.
- Presentation waits are bounded.
- The compositor can continue from known-good content rather than flashing blank frames during temporary producer loss.

---

## D3D11 state preservation

Prisma captures/restores the D3D11 state it modifies around its own rendering work, including the relevant portions of:

- render targets
- depth state
- input layout
- vertex buffers
- index buffers
- shaders
- constant buffers
- shader resources
- samplers
- blend state
- depth/stencil state
- raster state
- viewports
- scissor rectangles
- predication

This reduces the chance that Prisma rendering leaks state into Fallout or another graphics component.

---

## Freeze and crash diagnostics

- Added structured freeze-watchdog reporting.
- Fallout Present activity is tracked independently from Ultralight owner-thread activity.
- Diagnostics report the browser-owner operation active at the time of a stall.
- Save/load suppression prevents ordinary load screens from being mislabeled as Prisma freezes.
- Freeze dumps can be written under the Prisma F4SE diagnostics directory.
- D3D11 breadcrumbs capture the current generation, operation, relevant resource IDs, device/context identity, and graphics objects involved in accelerated failures.
- Renderer-health information includes focused-view health, focus ownership, pause ownership, input ownership, and compositor activity.

---

## Network and runtime security

PrismaUI remains a local game-interface framework, not a general web browser.

- Local documents are resolved through a contained logical path policy.
- Local HTML receives Prisma's generated Content Security Policy.
- External top-level navigation is rejected at the view/load boundary.
- Native listeners are not installed into untrusted remote documents.
- Remote scripts/styles/fonts/frames/media/network APIs are restricted by the framework's local-content security model.
- Network-policy tests cover scripts, stylesheets, fonts, frames, media, XHR/fetch, WebSockets, workers, forms, redirects, and related behavior.
- Consumer mods should package required frontend assets locally.
- General-purpose internet/network work belongs in native plugin code and should be bridged into the view intentionally.

---

## Fallout runtime support

### Standard Fallout 4

Supported runtime families for the current public framework:

- **Fallout 4 1.10.163**
- **Fallout 4 1.11.137+** where matching Address Library/runtime data exists

The intermediate Fallout 4 **1.10.980-1.10.984** Next-Gen line is not part of the current standard runtime policy.

### Fallout 4 VR

- VR remains a separate provider/package path.
- VR builds share current framework source where supported.
- VR keeps its own compositor and world-space presentation behavior.
- Flat-only hook-recovery logic remains excluded from the VR renderer where inappropriate.

---

## Developer and build improvements

- `xmake.lua` remains the native build-system source of truth.
- Framework versioning is sourced from one authoritative version definition.
- Flat/VR build metadata is kept aligned with the intended framework release.
- Public API headers are validated against the released framework source.
- Public Papyrus source and compiled PEX are kept synchronized with release output.
- Native policy/static tests cover view lifecycle, hook ownership, runtime packaging, network policy, input behavior, graphics generation handling, and release configuration.
- Native CI builds the standard framework, diagnostics variants, VR where applicable, and renderer validation configurations.
- Debug symbols are retained separately for native crash analysis.
- Release packaging verifies the final ZIP contents and checksums before publication.

---

## Public developer repository and documentation

The public developer surface was rebuilt alongside 2.1.0.

- API reference updated to the released V1-V10 header.
- Getting Started rewritten for the Ultralight runtime.
- View Lifecycle rewritten for the current managed owner model.
- HTML Views updated for local-content/runtime behavior.
- Networking guide updated for the local-content security model.
- View Health documentation corrected.
- Panel coordination and vanilla-menu suppression documentation corrected.
- ModelPreview guide rewritten to the actual released bridge.
- Papyrus Bridge guide rewritten to the actual ownership and error model.
- Translation timing corrected across the API and examples.
- Designer export paths corrected.
- Example plugin headers synchronized to the release.
- MCP tooling updated to report the current framework release and byte-pinned public SDK mirror.
- Docusaurus website and generated AI/LLM index updated to the 2.1.0 contract.

---

# PrismaUI_F4 2.0.9

**2026-08-23**

<details>
<summary><strong>Stability hotfix</strong></summary>

- Fixed an interface-load failure that could later incorrectly report itself as ready because of delayed browser callbacks.
- A failed load now remains failed until a valid replacement load succeeds.
- Corrected framework version reporting in logs so the reported PrismaUI version matches the installed release.
- Corrected VR-side version reporting that previously used stale build metadata.

</details>

---

# PrismaUI_F4 2.0.8

**2026-08-22**

<details>
<summary><strong>Startup crash hotfix</strong></summary>

- Fixed a startup/loading crash caused by an empty browser address reaching the browser runtime while a page was still being constructed.
- Empty addresses are now rejected/handled before reaching the browser engine.
- The crash could appear close to graphics-wrapper log messages, but those components were not the root cause of this specific failure.

</details>

---

# PrismaUI_F4 2.0.7

**2026-08-22**

<details>
<summary><strong>Performance, AE support, ModelPreview, Frame Generation</strong></summary>

### Performance

- Removed a renderer wait that could make Fallout wait for the next Prisma frame.
- Static menus/HUD elements reduced repeated work when nothing changed.
- Reduced repeated layout/size calculations for overlays.
- Budgeted ModelPreview work per frame.
- Reduced unnecessary cross-input processing between mouse and keyboard paths.

### Stability

- Fixed a workshop-menu hang with many item previews active.
- Added a hard ModelPreview resident limit.
- Bounded the preview loading queue.
- Cancelled preview jobs cannot later apply to the wrong item.
- Preview render targets are created atomically before they reach the renderer.
- Preview worker work is drained during shutdown.

### Anniversary Edition

- Updated runtime support for Fallout 4 1.11.240-era development.
- Restored textured item previews through a safer engine-thread loading path.
- Version-specific features disable themselves when exact engine data is unavailable.

### Graphics wrappers

- Improved detection of graphics-wrapper ownership.
- Improved behavior when another tool takes over presentation.
- Improved backbuffer/resource ownership handling.
- Improved recovery after resolution/display-mode changes.

</details>

---

# PrismaUI_F4 2.0.6

**2026-08-17**

<details>
<summary><strong>Stability, Frame Generation, VR, security, input</strong></summary>

### Stability

- Improved failed-view recovery.
- Improved close/reopen reliability.
- Reduced renderer synchronization stalls.
- Added additional stuck-focus and stuck-pause recovery.
- Improved delayed-event rejection for destroyed views.

### Frame Generation

- Improved blank-interface recovery with DLSS/FSR Frame Generation configurations.
- Improved swapchain replacement handling.
- Improved render-target ownership detection around graphics wrappers.

### Input and focus

- Improved controller routing.
- Improved horizontal mouse-wheel support.
- Separated pause ownership from focus-menu visibility.
- Added explicit Papyrus focus/unfocus support.

### VR

- Improved world-space panel rendering.
- Added scene-depth fallback behavior.
- Improved pointer visibility and VR resource cleanup.

### Security

- Tightened remote-resource handling.
- Removed general remote font/stylesheet assumptions.
- Improved isolation between local mod UI and external content.

</details>

---

# 2026-08-11 development update

<details>
<summary><strong>Framework hardening</strong></summary>

- Improved origin/local-file/network isolation.
- Improved native-message validation.
- Improved callback ownership.
- Improved browser startup/shutdown handling.
- Fixed Dock mouse behavior over Fallout's pause UI.
- Improved external-link handling under Wine/Proton.
- Improved VR pause maintenance and recovery.

</details>

---

# 2026-08-10 development update

<details>
<summary><strong>Overlay input regions</strong></summary>

- Added partial-screen overlay interaction.
- Transparent portions of an overlay can pass mouse input through to Fallout.
- Existing full-screen capture behavior remains available where required.
- Added capability discovery so plugins can feature-detect the newer input-region system.

</details>

---

# 2026-08-09 development update

<details>
<summary><strong>VR and SDK packaging</strong></summary>

- Improved public API provider lookup for standard Fallout and VR providers.
- Added separate VR API/developer packaging.
- Added stronger package freshness and export checks.
- Continued early real-headset VR development and controller testing.

</details>

---

# 2026-08-08 development update

<details>
<summary><strong>Input, lifecycle, health, Linux/Proton</strong></summary>

- Fixed controller input leaking to Fallout underneath Prisma UI.
- Fixed cursor-confinement recovery after Pip-Boy/menu interaction.
- Prevented Prisma from accepting input before a view was ready.
- Fixed Alt+F4 handling while a Prisma view owned focus.
- Fixed mesh-view wheel coordinates.
- Improved view-load failure and JavaScript-error reporting.
- Improved startup deadlock handling around early view creation.
- Added early software rendering fallback work for Wine/Proton configurations.

</details>

---

# PrismaUI 2.0 Beta

<details>
<summary><strong>Historical architecture</strong></summary>

The 2.0 beta generation temporarily moved Prisma from its original Ultralight runtime to CEF.

That work established or expanded many systems that survived into later versions, including:

- versioned C++ API interfaces
- native/JavaScript interop
- focus and input management
- controller support
- on-mesh interfaces
- ModelPreview
- view health and recovery concepts
- local-content security
- release packaging and API validation

The browser backend itself was later replaced again in 2.1.0 after the multiprocess runtime proved heavier and more failure-prone than needed for local Fallout UI.

Current users and developers should follow the 2.1.0 Ultralight documentation rather than historical 2.0 CEF setup instructions.

</details>

---

# PrismaUI 1.8

<details>
<summary><strong>Memory, watchdog, transparent click-through</strong></summary>

### Stability and memory

- Tuned Ultralight memory configuration to reduce per-view memory use.
- Reduced JavaScriptCore heap sizing.
- Reduced WebCore cache sizing.
- Disabled unnecessary page cache behavior.
- Capped renderer thread count.
- Reduced out-of-memory pressure in heavy load orders.

### Reliability watchdog

- Added periodic health summaries for hosted views.
- Added fault/quarantine handling for repeatedly failing views.
- Hidden-view render buffers can be reclaimed under memory pressure.

### Input

- Added transparent-region click-through.
- Preserved drag capture when a drag begins over real UI.
- Fixed focus-menu modal behavior that allowed underlying menus to steal input.

</details>

---

# PrismaUI 1.7

<details>
<summary><strong>Framework, tooling, Papyrus, documentation</strong></summary>

- Added the NotificationSystem.
- Improved InputHandler reliability.
- Improved view lifecycle architecture.
- Improved C++/JavaScript/game communication.
- Finalized xmake as the supported build system.
- Removed legacy CMake support.
- Consolidated local build/deploy tooling.
- Added additional deployment validation.
- Improved example-plugin Event Log and callbacks.
- Improved Papyrus FormID/property resolution.
- Expanded documentation, tutorials, limitations, translations, and modern-framework guidance.

</details>

---

# PrismaUI 1.6

<details>
<summary><strong>Papyrus bridge and NewCommonLib migration</strong></summary>

### Papyrus bridge

- Added automatic `window.prisma` injection.
- Added TESGlobal read/write support.
- Added supported Papyrus Auto-property read/write flows.
- Added promise-based reads and fire-and-forget writes.
- Added plugin-aware FormID resolution.

### Toolchain

- Migrated the example plugin and core framework to the xmake/NewCommonLib development stack.
- Removed older CMake/CommonLib project wiring.
- Updated plugin entry points and logging for the current CommonLib-based stack.

</details>

---

# PrismaUI 1.5

<details>
<summary><strong>Network sandbox</strong></summary>

- Added automatic network sandboxing for Prisma views.
- Restricted outbound network APIs.
- Restricted `CreateView` to local framework views.
- Blocked unwanted external navigation.
- Added logging for blocked network/navigation attempts.

</details>

---

# PrismaUI 1.4

<details>
<summary><strong>Documentation and V4 API</strong></summary>

- Updated API docs/examples for V4.
- Added `BindUIEvent` documentation.
- Expanded translation documentation.
- Added plugin-update/migration guidance.
- Expanded DOM-ready and JavaScript event examples.

</details>

---

# PrismaUI 1.3

<details>
<summary><strong>Initial framework foundation</strong></summary>

- Added Fallout runtime/API registration groundwork.
- Added Address Library integration.
- Added the original Ultralight HTML/CSS/JavaScript rendering layer.
- Added versioned public plugin API exports.
- Added core view lifecycle methods.
- Added `InteropCall`, `Invoke`, and `RegisterJSListener`.
- Added JavaScript console callbacks.
- Added translation registration.
- Added early inspector/debugging compatibility APIs.

</details>

---

# PrismaUI 1.2

<details>
<summary><strong>PrismaDesigner refactor</strong></summary>

- Refactored canvas item handling.
- Moved inserted elements to a reference-based internal model.
- Performed foundational stability and architecture cleanup for later Designer work.

</details>
