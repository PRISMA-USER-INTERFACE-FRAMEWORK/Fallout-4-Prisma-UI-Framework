# PrismaUI_F4 2.2.0

PrismaUI_F4 is an F4SE framework for building Fallout 4 interfaces with HTML, CSS, and JavaScript. Version 2.2.0 uses an in-process Ultralight 1.4.0 runtime, the stable V1-V12 ABI, and a preferred modern feature-table API for menus, HUDs, settings panels, terminals, inventory interfaces, and other mod UI.

## Runtime support

- Fallout 4 1.10.163
- Fallout 4 1.11.137+ with matching Address Library data
- The intermediate 1.10.980-1.10.984 runtime line is not supported

## Developer surface

This repository contains the public PrismaUI_F4 developer surface:

- stable desktop ABI header, `src/PrismaUI_F4_API.h`, containing `IVPrismaUI1` through `IVPrismaUI12`
- preferred feature-table header, `src/PrismaUI_F4_Modern_API.h`, with Core, Controller, GameThread, Meta, View, Interop, Localization, Render, Input, and Menu tables
- V4 JSON localization through Fallout resources with loose/BA2 support and English fallback
- native Gamepad mode plus focused controller actions and framework-owned bridge recovery
- restored in-game Ultralight DevTools on F12
- controller action guide for A/B/X/Y, shoulders, triggers, stick clicks, Start/Back, and D-pad actions
- Papyrus source and compiled PEX
- API reference and integration guides
- F4SE example plugin
- Prisma Designer integration guides
- ModelPreview API v4 documentation
- MCP server
- Docusaurus documentation website
- CI that validates SDK/Papyrus mirrors, MCP dependencies, and the website build

For new integrations, prefer `PrismaUI_F4_Modern_API.h` and discover only the feature tables you need. Existing numbered-interface consumers should keep using `PrismaUI_F4_API.h` and request the lowest V1-V12 interface containing the required feature.

## Public download

https://www.nexusmods.com/fallout4/mods/105454

## Documentation

https://prisma-user-interface-framework.github.io/Fallout-4-Prisma-UI-Framework/

## Quick paths

- `src/PrismaUI_F4_API.h` - stable desktop C++ ABI, V1-V12
- `src/PrismaUI_F4_Modern_API.h` - preferred feature-table discovery API
- `src/PrismaUI_F4VR_API.h` - VR provider/header contract
- `scripts/Source/PrismaUI.psc` - Papyrus source
- `docs/getting-started.md` - plugin setup
- `docs/modern-api.md` - current feature-table API and discovery guide
- `docs/api-reference.md` - V1-V12 compatibility API overview
- `docs/api-extensions.md` - V11/V12 contract and ABI notes
- `docs/2.2.0-release.md` - current release changes
- `docs/controller-actions.md` - V12 controller support guide
- `docs/model-preview.md` - ModelPreview API v4
- `docs/papyrus-bridge.md` - page-to-Papyrus bridge
- `mcp-server/` - MCP tooling
- `website/` - documentation site

## License

See `LICENSE.md` and the third-party notices included in the project.
