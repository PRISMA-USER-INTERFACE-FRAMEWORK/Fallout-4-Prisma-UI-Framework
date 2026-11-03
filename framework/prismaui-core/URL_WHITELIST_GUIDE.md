# PrismaUI_F4 network allowlists

This is the operator guide for `src/PrismaUI/URLWhitelist.h` and
`src/PrismaUI/WebNetworkPolicy.h`. The citable security description is
`docs/NETWORK_SAFETY_AUDIT.md`. If the two disagree, the headers and the audit
win; this file is not an independent policy.

There is **no** single universal domain array. Hosts are capability-specific.
Remote script is categorically denied for trusted local Prisma documents.
`file:` is not a CSP source; a `file://` top-level document is rejected at the
load/navigation callback boundary (`UltralightDocumentCallbacks`).

## Enforcement layers

1. **Generated CSP** — the enforcement surface for a trusted local Prisma
   document. `PrismaFileSystem.cpp:106-107` injects it into every document load
   from the per-capability arrays in `URLWhitelist.h`, so a host is reachable
   only if its array lists it.
2. **Top-level document identity** — `UltralightDocumentCallbacks` calls
   `WebNetworkPolicy::IsTrustedLocalDocumentUrl` to decide whether a load is the
   view's own local document, and blocks navigation when it is not. This is a
   whole-document check, not a per-request one: it governs *which document* a
   view may show, not which subresources that document may fetch.
3. **In-page fetch/XHR wrapper** (`NetworkSandbox::GenerateScript`) — **not
   currently injected.** It has no caller in `src/`, so it is not part of the
   live policy. The connect list it would use is the **empty**
   `GenerateJsConnectWhitelistArray()` (→ `[]`), and it does not reuse the
   image/frame/media arrays.

Those layers are not “one universal list.” Adding a host to `kImageDomains` does
not allow scripts, stylesheets, fonts, or `fetch`.

`WebNetworkPolicy` also declares `IsAllowedRemoteEmbed`,
`AllowPublicHttpsAfterCoreChecks`, `DenyTrustedLocalRemoteScript` and
`FrameClassName`. **None of them has a call site outside `WebNetworkPolicy.h`**,
so none currently enforces anything. Do not cite them as a boundary: only
`IsTrustedLocalDocumentUrl` is wired.

## Trusted local Prisma frames

| Capability | Arrays | Current hosts |
|---|---|---|
| Script / document / unknown | none | denied |
| Connect (`fetch` / XHR) | `kConnectDomains` | empty |
| Stylesheet | `kStylesheetDomains` | empty (self-host CSS) |
| Font | `kFontDomains` | empty (self-host fonts) |
| Image | `kImageDomains` | `static.wikia.nocookie.net` |
| Media / frame | `kMediaDomains` / `kFrameDomains` | `youtube.com`, `youtu.be`, `googlevideo.com` |
| External navigation (system browser, not fetched) | `kExternalNavigationDomains` | `nexusmods.com` |

A JavaScript CDN is **not** allowlisted. Nexus is **not** a resource origin;
it is only an external-navigation destination. YouTube is an embed/frame host, not
an external-browser destination. `ClassifyExternalUrl` consults this list; opening
still requires a trusted Prisma source and a user gesture. Video
embeds use `IsAllowedVideoEmbedUrl` (`kFrameDomains`) instead.
`kLoopbackVideoProxyCspFrameSource` (`http://127.0.0.1:*`) is appended to
generated `frame-src` **only for `CspTransport::kLegacyCompatibility`**; the live
injector passes `kUltralight` (`NetworkSandbox.h:74`), so the shipping provider's
CSP does **not** carry a loopback source. The native proxy that enforced an
exact-port `/embed` boundary was deleted with the CEF backend, so nothing native
backstops that source either.

## Remote nested embeds

A YouTube iframe attributed to a Prisma view is **not** a trusted local
document. Remote embeds are governed by the same generated CSP: the remote-embed
host arrays in `WebNetworkPolicy.h` feed the directives, and the top-level
document check keeps a remote page from being the view's own document.

`WebNetworkPolicy::IsAllowedRemoteEmbed` looks like the remote-embed boundary,
and its host lists are the capability data CSP is built from, but **the function
itself has no call site outside `WebNetworkPolicy.h`**. It does not currently
run, so it cannot be cited as the check that stops a fan-out.

## Adding a host

1. Put it in the **capability array that matches the resource type**.
2. Rebuild and redeploy `PrismaUI_F4.dll` (and `PrismaUI_F4VR.dll` for the VR provider).
3. Add a `tests/core_security_tests.cpp` assertion for that capability and a
   negative test that a different capability stays denied.

Do not add remote script hosts for trusted local views. Do not put Google Fonts
back on `kStylesheetDomains` / `kFontDomains`.

## Self-hosting fonts

Bundle a `woff2`/`ttf` next to the view and reference it with `@font-face`.
Do not `<link>` or `preconnect` `fonts.googleapis.com` / `fonts.gstatic.com`.

## Rebuild

```powershell
cd framework/prismaui-core
xmake
```

xmake is the only build system. cmake/vcpkg are retired.

## What this guide used to get wrong

Older revisions described a single universal domain list, claimed a JavaScript CDN and
Nexus resources were allowlisted, said both JS and CSP consumed that one list, showed
`file:` in generated CSP, and treated `ClassifyExternalUrl` as “any public HTTPS.”
External navigation is `kExternalNavigationDomains` (Nexus) plus trusted source and
user gesture. None of the older claims match the implementation.
