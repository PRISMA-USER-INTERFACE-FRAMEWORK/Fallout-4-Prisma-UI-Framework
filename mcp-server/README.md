# prisma-mcp

`prisma-mcp` 1.1 gives an MCP-compatible client structured access to the current **PrismaUI_F4 2.2.0** modern, compatibility, and VR developer surfaces.

The current framework uses **Ultralight 1.4.0 in-process**. New desktop integrations should prefer the `PrismaUI_F4_Modern_API.h` feature-table surface; the canonical `PrismaUI_F4_API.h` V1-V12 header remains the compatibility ABI. Desktop support covers OG 1.10.163 plus AE 1.11.137+ with matching Address Library data; the intermediate 1.10.980-1.10.984 line is deliberately unsupported.

## Requirements

- **Node.js 20 or newer**
- Git for the source-build install path

## Source of truth

Prisma-Matrix is the maintainer source repository. Public MCP users do **not** need to read Matrix directly to use the SDK or developer docs because the public framework repository carries pinned SDK mirrors and current guides.

The public contract is:

- **Fallout-4-Prisma-UI-Framework `main`** for guides, method documentation, examples, and distributable SDK mirrors.
- `src/PrismaUI_F4_Modern_API.h` is the preferred feature-table header for new desktop integrations.
- Its pinned Git blob is `9784daab39bf66bb179e6ef12bba63c75e50d3b7`.
- `src/PrismaUI_F4_API.h` contains the stable `IVPrismaUI1` through `IVPrismaUI12` compatibility ABI.
- Its pinned Git blob is `02f6584829063ca59662d135c6910cc87d5bb2ee`.
- Its maintainer-side API source is Prisma-Matrix release commit `1b32eb1fef28802b35cc9d43ad44152d769bebd5`.
- The 2.2.0 release source is `1b32eb1fef28802b35cc9d43ad44152d769bebd5`; the public SDK mirrors are pinned to that release contract.
- The VR header mirror remains Git blob `012f810a98bfa274563c9eb8102881549ab2c9c6`.
- Repository CI checks the public SDK blob identities before documentation changes can merge.

`get_modern_header`, `get_header`, and `get_vr_header` download the preferred modern, V1-V12 compatibility, and VR SDK mirrors and recompute their Git blob SHAs locally. All fail closed on drift.

`get_framework_release` verifies all three SDK snapshots before returning the 2.2.0 release identity, renderer, general desktop runtimes, narrower native-controller runtimes, modern feature list, public download URL, and SDK provenance.

Public framework download:

https://www.nexusmods.com/fallout4/mods/105454

Maintainer provenance, when Matrix access is available:

https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Prisma-Matrix/releases/tag/framework-v2.2.0

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
- 2.2.0 release source commit;
- current canonical API source commit;
- Ultralight/renderer identity;
- supported desktop runtime families;
- deliberately rejected runtime line;
- public release URL plus maintainer provenance;
- preferred modern, compatibility, and VR API-header mirror URLs and pinned Git blob SHAs;
- native-controller runtime boundary for OG 1.10.163 and AE 1.11.240;
- released modern feature tables;

### `get_modern_header`

Returns the verified `PrismaUI_F4_Modern_API.h` feature-table header. Use this first for new integrations.

### `list_modern_features`

Parses the verified modern header and returns every released feature table with its numeric feature ID, table version, members, typedef names, and resolved function-pointer signatures.

### `get_modern_feature`

Returns one modern feature table by name. Use this for exact signatures and membership before generating feature-table integration code.

### `get_header`

Returns the verified `PrismaUI_F4_API.h` V1-V12 compatibility header. Use it when maintaining numbered-interface integrations.

### `get_vr_header`

Returns the verified `PrismaUI_F4VR_API.h` release header for spatial presentation, pointer routing, network policy, capabilities, and VR provider integration.

### `list_api_methods`

Lists documented numbered V1-V12 compatibility API methods and their interface versions. The optional `sinceVersion` filter accepts values such as `V10`, `V11`, or `V12`.

### `get_api_method`

Returns the detailed documentation page for one numbered compatibility method. Use `get_modern_feature` for modern-only members such as `RegisterTranslationsV4` or `SetNativeGamepad`.

### `search_docs`

Searches compatibility method docs, current guides, and the verified modern and VR SDK contracts by keyword.

### `get_guide`

Returns one current guide. The guide catalog includes setup, networking, translations, panel management, lifecycle, troubleshooting, ModelPreview, controller actions, API extensions, the API reference, the modern API guide, and the 2.1.1/2.2.0 release guides.

### `scaffold_plugin`

Creates a consumer project in a user-confirmed local path. `apiStyle=modern` is the default and generates native code using View, Interop, and Controller feature tables with verified modern and compatibility headers. `apiStyle=legacy` preserves the numbered-interface example.

## Recommended agent workflow

For a new PrismaUI task:

1. call `get_framework_release`;
2. call `get_modern_header` for new integrations;
3. call `list_modern_features` or `get_modern_feature` for exact modern table membership and signatures;
4. call `get_vr_header` for VR spatial/provider work;
5. use `get_header` only for numbered V1-V12 compatibility work;
6. search or fetch the relevant guide, including `translations` for V4 localization;
7. use `get_api_method` for legacy methods with lifecycle/runtime caveats;
8. scaffold only after the user confirms the destination path and prefer the default modern style;
9. do not invent retired CEF/subprocess setup, unsupported NG compatibility, or remote browser features.

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
