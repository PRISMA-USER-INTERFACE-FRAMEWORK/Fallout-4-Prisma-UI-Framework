# PrismaUI_F4 Build Instructions

PrismaUI_F4 uses the pinned Ultralight 1.4.0 SDK. There is no second browser engine: the retired CEF
backend has been removed from the repository outright, not merely excluded from packages.

## Requirements

- Visual Studio 2022 with the C++ toolchain (provides the MSVC runtime and Windows SDK)
- Standalone LLVM at `C:\Program Files\LLVM\bin`, or `PRISMA_LLVM_BIN` pointing at an equivalent
  directory; it must provide `clang-cl.exe`, `lld-link.exe`, `llvm-ar.exe`, `llvm-rc.exe`, and
  `llvm-dlltool.exe`
- xmake 3.0+
- CMake and Ninja from Visual Studio, plus the Windows SDK shader compiler
- Python 3
- CommonLibF4 submodule initialized
- CommonLibF4VR submodule initialized when building the VR provider

```batch
git submodule update --init --recursive
```

## 1. Prepare Ultralight

Provide the pinned SDK headers and libraries under `external/ultralight-1.4.0/`. Release packaging
requires `ULTRALIGHT_RUNTIME_DIR` pointing to a verified runtime directory containing the pinned
DLLs, resources, and legal payload. An SDK archive may be supplied explicitly with
`ULTRALIGHT_SDK_ARCHIVE` when staging locally.

## 2. Build the shared web assets

```batch
cd shell
npm ci
npm run build
cd ..
```

A successful build produces `shell/dist/index.html`.

## 3. Build the flat shipping binaries

On Windows the canonical provider build path is the repository helper. It selects the toolchain,
prepares the xwin junctions, records the cache fingerprint, builds through `xmake-xwin-framework.lua`,
and validates the resulting PE before reporting success:

```powershell
.\scripts\setup-windows-clang-toolchain.ps1
.\scripts\build-windows-framework.ps1 -Target PrismaUI_F4
```

The setup step requires the **standalone LLVM** toolchain and never falls back to another `PATH`
clang. It uses `PRISMA_LLVM_BIN` when set, otherwise `C:\Program Files\LLVM\bin`, and fails before
configure unless that directory contains all of `clang-cl.exe`, `lld-link.exe`, `llvm-ar.exe`,
`llvm-rc.exe`, and `llvm-dlltool.exe`. A Visual Studio–bundled LLVM is not an accepted substitute,
and the build fingerprint records the resolved `clang-cl`/`lld-link` paths and versions so a
different LLVM installation cannot reuse another installation's cached objects.
The helper also builds DirectXTK 24.2.0 from its checksum-pinned source with that compiler,
verifies the compiler records in `DirectXTK.lib`, and includes the library hash in the provider
cache fingerprint. Both Windows and WSL/xwin builds use `PRISMA_DIRECTXTK_ROOT` to select it.

Ad-hoc `xmake config` / `xmake build` invocations configured by hand are not the shipping path and
are not equivalent to the helper's output. Treat the helper as authoritative and reproduce a
provider load through `.github/scripts/flat-load-smoke.py` before trusting a DLL.

Required outputs:

- `PrismaUI_F4.dll`
`PrismaUI_F4.dll` is the flat provider. The Ultralight DLLs, resources, and legal files are staged
beside the framework assets rather than linked into additional provider binaries.

To exercise the loader against a built provider:

```powershell
python .github\scripts\flat-load-smoke.py <build-dir>\windows\x64\release\PrismaUI_F4.dll
```

## 4. Build the VR provider

The same helper selects a separate VR build directory:

```powershell
.\scripts\build-windows-framework.ps1 -Target PrismaUI_F4VR
```

Required output:

- `PrismaUI_F4VR.dll`

## Validation

Run the repository policy/static/unit gate on Windows:

```powershell
python tests/ci_windows_gate.py
```

The GitHub PR CI additionally runs the native shipping compile and C++ test gates.

## Release packaging

Release packaging is owned by `scripts/ci-package-release.py`. The release workflow's `prepare`
operation builds once, verifies both packages, records all four DLL/ZIP hashes in
`SHA256SUMS.txt`, and creates an unpublished draft release candidate. Test the exact desktop ZIP
from that draft. The `publish` operation rebuilds nothing and publishes only after the supplied
runtime DLL/ZIP hashes match the retained candidate and legal sign-off is explicit.

Each package includes the PrismaUI license plus the unchanged Ultralight license, EULA, and notices
under `Data/PrismaUI_F4/licenses/`.

The workflow always compiles and packages both providers, but the draft candidate includes only the
flat `Fallout4-OG-AE` package by default. Set `PRISMA_PUBLISH_VR=true` before preparing a candidate
only after a separate validated VR release decision. Existing candidates are never overwritten.

The packaging scripts reject `libcef.dll` and `PrismaUI_F4_CefSubprocess.exe` in a production archive.
That check is permanent: the CEF sources are gone, so the only way one could reappear is a regression
that reintroduces them.

## Runtime notes

- Release packaging copies the tracked `release-assets/PrismaUI_F4.ini`; mesh binding is off by default (`[Debug] bMeshBinding=0`). Enable it only for deliberate OG/AE runtime testing of Pip-Boy / terminal screens.
- `[DevTools] bEnabled=1` exposes an unauthenticated loopback protocol. Development only.
