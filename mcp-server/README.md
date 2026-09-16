# prisma-mcp

`prisma-mcp` gives an MCP-compatible client structured access to the current **PrismaUI_F4 2.1.0** developer surface.

The current framework uses **Ultralight 1.4.0 in-process**. The desktop API is exposed through one canonical `PrismaUI_F4_API.h` SDK header containing **V1 through V12**. Desktop support covers OG 1.10.163 plus AE 1.11.137+ with matching Address Library data; the intermediate 1.10.980-1.10.984 line is deliberately unsupported.

## Requirements

- **Node.js 20 or newer**
- Git for the source-build install path

## Source of truth

The implementation repository, Prisma-Matrix, is private. Public MCP users do **not** need Matrix credentials to read the SDK or developer docs.

The public contract is:

- **Fallout-4-Prisma-UI-Framework `main`** for guides, method documentation, examples, and distributable SDK mirrors.
- `src/PrismaUI_F4_API.h` is the single desktop SDK header and contains `IVPrismaUI1` through `IVPrismaUI12`.
- The current canonical desktop SDK header is Git blob `5c03467ce567921e1de86ef89157cd246e07c977`.
- Its maintainer-side API source is Prisma-Matrix commit `c2892083329db9f255191a052c3b8b922c4e27b1`.
- The original 2.1.0 release source remains `061f699864500cd754c9aac854eb047093a161ea`; MCP reports release provenance and current SDK provenance separately.
- The VR header mirror remains Git blob `8221eb7bd81694f604b6f188fc8b2c475200dbf0`.
- Repository CI checks the public SDK blob identities before documentation changes can merge.

`get_header` downloads the public `src/PrismaUI_F4_API.h` mirror and recomputes its Git blob SHA locally. It fails closed if the bytes do not match the canonical V1-V12 SDK header.

`get_framework_release` verifies that public SDK snapshot before returning the 2.1.0 release identity, backend, renderer, supported runtime matrix, original release source commit, current API source commit, and SDK blob.

Public framework download:

https://www.nexusmods.com/fallout4/mods/105454

Maintainer provenance, when Matrix access is available:

https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Prisma-Matrix/releases/tag/framework-v2.1.0

## Install

```text
git clone https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Fallout-4-Prisma-UI-Framework.git
cd Fallout-4-Prisma-UI-Framework/mcp-server
npm install
npm run build
node dist/index.js
```

Configure your MCP client to launch `node /full/path/to/Fallout-4-Prisma-UI-Framework/mcp-server/dist/index.js`.

## GitHub rate limits

The MCP server reads the public Fallout-4-Prisma-UI-Framework repository at runtime and caches responses in memory. A `GITHUB_TOKEN` is optional and is only useful for higher GitHub API rate limits.

## Tools

### `get_framework_release`

Call this first when version, backend, renderer, Fallout runtime support, or compatibility matters. It returns:

- framework version and release tag;
- original 2.1.0 release source commit;
- current canonical API source commit;
- Ultralight/renderer identity;
- supported desktop runtime families;
- deliberately rejected runtime line;
- public release URL plus maintainer provenance;
- public V1-V12 API-header mirror URL and pinned Git blob SHA.

### `get_header`

Returns the public mirror of the canonical `PrismaUI_F4_API.h` after verifying its Git blob SHA. Use it before writing or reviewing native API calls. This one header contains V1-V12.

### `list_api_methods`

Lists documented API methods and their interface versions. The optional `sinceVersion` filter accepts values such as `V10`, `V11`, or `V12`.

### `get_api_method`

Returns the detailed documentation page for one method, including signatures, parameters, lifecycle/threading notes, and examples where documented.

### `search_docs`

Searches API method documentation and guides by keyword.

### `get_guide`

Returns one current guide. The guide catalog includes setup, networking, panel management, lifecycle, troubleshooting, ModelPreview, controller actions, API extensions, the API reference, and the 2.1.0 release guide.

### `scaffold_plugin`

Copies and renames the official example consumer plugin into a user-confirmed local path. The scaffold replaces its desktop API header with the same verified canonical V1-V12 mirror before writing the project.

## Recommended agent workflow

For a new PrismaUI task:

1. call `get_framework_release`;
2. call `get_header` before generating C++;
3. include `PrismaUI_F4_API.h` for desktop code;
4. request the lowest interface version containing the needed feature;
5. search or fetch the relevant guide;
6. use `get_api_method` for methods with lifecycle/runtime caveats;
7. scaffold only after the user confirms the destination path;
8. do not invent retired CEF/subprocess setup, unsupported NG compatibility, or remote browser features.

## Development

```text
npm ci
npm run build
npm run verify:release
npx @modelcontextprotocol/inspector node dist/index.js
```

CI also runs `npm audit --omit=dev --audit-level=high` against the production dependency graph.

## License

MIT for the MCP server code. PrismaUI_F4 documentation, API headers, examples, framework code, and third-party runtime components remain governed by their respective licenses.
