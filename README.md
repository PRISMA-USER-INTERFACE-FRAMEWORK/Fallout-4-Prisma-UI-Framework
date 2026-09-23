# PrismaUI_F4 2.1.0

PrismaUI_F4 is an F4SE framework for building Fallout 4 interfaces with HTML, CSS, and JavaScript. Version 2.1.0 uses an in-process Ultralight 1.4.0 runtime and a versioned native API for menus, HUDs, settings panels, terminals, inventory interfaces, and other mod UI.

## Runtime support

- Fallout 4 1.10.163
- Fallout 4 1.11.137+ with matching Address Library data
- The intermediate 1.10.980-1.10.984 runtime line is not supported

## Developer surface

This repository contains the public PrismaUI_F4 developer surface:

- one desktop C++ SDK header, `src/PrismaUI_F4_API.h`, containing `IVPrismaUI1` through `IVPrismaUI12`
- V11 verified Fallout window-thread dispatch and game-thread UI bindings
- V12 focused controller-action routing
- controller action guide for A/B/X/Y, shoulders, triggers, stick clicks, Start/Back, and D-pad actions
- Papyrus source and compiled PEX
- API reference and integration guides
- F4SE example plugin
- Prisma Designer integration guides
- ModelPreview API v4 documentation
- MCP server
- Docusaurus documentation website
- CI that validates SDK/Papyrus mirrors, MCP dependencies, and the website build

Always request and null-check the exact interface version your mod needs. Older installed providers may expose only an earlier API version. The SDK ships as one header; request the lowest interface containing the feature you need.

## Public download

https://www.nexusmods.com/fallout4/mods/105454

## Documentation

https://prisma-user-interface-framework.github.io/Fallout-4-Prisma-UI-Framework/

## Quick paths

- `src/PrismaUI_F4_API.h` - complete desktop C++ API, V1-V12
- `src/PrismaUI_F4VR_API.h` - VR provider/header contract
- `scripts/Source/PrismaUI.psc` - Papyrus source
- `docs/getting-started.md` - plugin setup
- `docs/api-reference.md` - current V1-V12 API overview
- `docs/api-extensions.md` - V11/V12 contract and ABI notes
- `docs/controller-actions.md` - V12 controller support guide
- `docs/model-preview.md` - ModelPreview API v4
- `docs/papyrus-bridge.md` - page-to-Papyrus bridge
- `mcp-server/` - MCP tooling
- `website/` - documentation site

## License

See `LICENSE.md` and the third-party notices included in the project.
