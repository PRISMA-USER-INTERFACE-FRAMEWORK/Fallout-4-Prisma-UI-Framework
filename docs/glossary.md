---
title: Glossary
sidebar_label: Glossary
sidebar_position: 20
---

# Glossary

Short definitions of the words this documentation uses. If a page uses a term you do not know, it is probably here.

## Game and modding

- **F4SE** — Fallout 4 Script Extender. The loader that runs native plugin DLLs and Papyrus extensions.
- **F4SE plugin** — a native `.dll` that F4SE loads from `Data\F4SE\Plugins`. Your mod's native half is one of these.
- **CommonLibF4** — the C++ library that gives plugins clean access to Fallout 4's engine classes.
- **BA2** — Fallout 4's archive format. Views and translations can be loose files or packed in a BA2.
- **FOMOD** — an installer definition used by mod managers. Optional, but nice for options.
- **MO2 / Vortex** — mod managers. They present a virtual `Data` folder; keep the normal `Data` layout inside your archive.

## PrismaUI

- **View** — one loaded HTML page managed by PrismaUI. It has a handle (`PrismaView` in C++, a name string in Papyrus).
- **Page** — the actual HTML document inside a view: `index.html`, CSS, JavaScript.
- **Bridge** — the connection between the page and the native side: `BindUIEvent`, `RegisterJSListener`, `InteropCall`, `Invoke`.
- **Interop call** — calling a JavaScript function by name with a string payload (usually JSON).
- **Invoke** — running a line of JavaScript in the page.
- **DOM ready** — the moment the page finished parsing and JavaScript can touch the document. PrismaUI calls your `OnDomReady` then.
- **Focus** — the view receives mouse and keyboard input. Only one view owns normal focus at a time.
- **Role** — what kind of view this is: `kPanel` (interactive) or `kWidget` (passive HUD). Used for cooperation between mods.
- **Input region** — a screen rectangle that receives clicks on an overlay-focused view; everything else passes through to the game.
- **Overlay focus** — focus that only owns the declared input regions, so the game keeps running.
- **Offscreen view** — a view rendered into a texture you can bind to geometry, used for in-world screens. Advanced.
- **View health** — the framework's state for a view: creating, DOM ready, live, failed, timed out, unresponsive, JS error.
- **Watchdog** — the recovery logic that releases input and pause if a focused view stops rendering.

## Runtime and rendering

- **Ultralight** — the in-process web engine PrismaUI uses. It is not Chrome: some browser features are unavailable.
- **D3D11** — the graphics API Fallout 4 uses. PrismaUI presents its views into the game's frame.
- **GPU path / CPU path** — PrismaUI renders accelerated when it can safely own the graphics state, and falls back to CPU rendering otherwise.
- **Compositor** — the part that blends PrismaUI's rendered view into Fallout's frame.
- **HUD** — a passive overlay drawn over gameplay. In PrismaUI, a `kWidget` view.

## Engine access

- **Game thread** — Fallout's main thread, where engine state must be touched. PrismaUI callbacks already run there.
- **Game-thread dispatch** — sending work from your own thread onto the game thread (`DispatchToGameThread`).
- **Papyrus** — Fallout's scripting language. PrismaUI exposes a small Papyrus script for creating and controlling views.

## API styles

- **Modern API** — `PrismaUI_F4_Modern_API.h`. You request feature tables (`View`, `Controller`, `Interop`, …) and use only what you asked for.
- **Numbered API** — `PrismaUI_F4_API.h` with `IVPrismaUI1` … `IVPrismaUI12`. The stable ABI existing plugins use.
- **Feature table** — one `Discover` call that returns a small set of related calls, for example all view calls.
- **Capability** — an optional behavior that a build may or may not enable, checked with `HasPrismaCapability`.

## Places

- **PrismaUI_F4.log** — `Documents\My Games\Fallout4\F4SE\PrismaUI_F4.log`. Framework log.
- **Views root** — `Data\PrismaUI_F4\views\`. Each plugin gets one folder there.
- **Translations folder** — `Data\Interface\Translations\`. JSON (`V4`) or `.txt` (legacy) files per language.

## Where to go next

- [What is PrismaUI F4?](what-is-prismaui) — the overview
- [Your first panel](quick-start) — the practical introduction
- [API reference](api-reference) — every call, with parameters
