-- PrismaUI_F4 / xmake.lua
--
--
-- Requirements:
--   1. CommonLibF4 -- see the Matrix build adapter note below.
--
-- The Ultralight runtime is staged from the pinned SDK archive by scripts/stage-ultralight-runtime.py.
--
-- Deployment (optional):
--   Set XSE_FO4_MODS_PATH to an MO2 mods folder root for local auto-deploy.
--

local PRISMA_VERSION = "2.2.0"

-- The version the BINARIES report, derived from the one above rather than typed a second time.
--
-- F4SE_PLUGIN_VERSION used to carry a literal: main.cpp said { 2, 2, 4, 0 } and VRMain.cpp said
-- { 1, 0, 0, 0 }, neither touched since. Every log line, every bug report and every "which build
-- is this?" answered with a number that had nothing to do with the release -- a 2.0.8 install
-- announced itself as v2.2.4.0, which cost a live debugging session on 2026-08-22 (the build was
-- confirmed correct only by hashing the DLL against the published zip).
local PRISMA_VERSION_MAJOR, PRISMA_VERSION_MINOR, PRISMA_VERSION_PATCH =
    PRISMA_VERSION:match("^(%d+)%.(%d+)%.(%d+)$")
if not PRISMA_VERSION_MAJOR then
    -- CI loads this file through xmake-xwin-framework.lua, i.e. xmake's --file DESCRIPTION scope,
    -- which exposes neither assert() nor raise() in the pinned version -- an assert() here failed
    -- every build with "attempt to call a nil value (global 'assert')". Print the reason, then
    -- call an undefined sentinel to abort configuration: the same shape, and the same reason, as
    -- the die() helper in xmake-xwin-framework.lua.
    print("error: PRISMA_VERSION must be x.y.z -- got: " .. tostring(PRISMA_VERSION))
    PRISMA_VERSION_IS_NOT_X_Y_Z()
end

option("prisma_accelerated_runtime_acceptance")
    set_default(false)
    set_showmenu(true)
option_end()

-- Root scope on purpose: PrismaUI_F4 and PrismaUI_F4VR are separate targets and BOTH report a
-- version. Adding this inside one target is how they drifted apart in the first place.
add_defines("PRISMA_VERSION_MAJOR=" .. PRISMA_VERSION_MAJOR,
            "PRISMA_VERSION_MINOR=" .. PRISMA_VERSION_MINOR,
            "PRISMA_VERSION_PATCH=" .. PRISMA_VERSION_PATCH)

if has_config("prisma_accelerated_runtime_acceptance") then
    add_defines("PRISMA_ACCELERATED_RUNTIME_ACCEPTANCE=1")
else
    add_defines("PRISMA_ACCELERATED_RUNTIME_ACCEPTANCE=0")
end

add_requires("minhook")
if not os.getenv("PRISMA_DIRECTXTK_ROOT") then
    add_requires("directxtk")
end
-- spdlog v1.16.0 propagates as public from commonlib-shared; declaring the same
-- version+configs here makes it available for direct inclusion in PCH.h.
add_requires("spdlog v1.16.0", { configs = { header_only = false, wchar = true, std_format = true } })

target("PrismaUI_F4")
    set_kind("shared")
    set_filename("PrismaUI_F4.dll")
    set_symbols("debug")

    -- Add CommonLibF4 dependency
    add_deps("commonlibf4")

    -- Sources
    add_includedirs("src")
    -- Ultralight 1.4 SDK (vendored under external/ultralight-1.4.0/{include,lib}). The accelerated
    -- GPUDriver (src/GPU/) compiles against the API and links AppCore/Ultralight/UltralightCore/WebCore
    -- delay-loaded (see below). See docs/ultralight-migration.md for the API cheat-sheet and source material.
    add_includedirs("external/ultralight-1.4.0/include")
    add_linkdirs("external/ultralight-1.4.0/lib")
    add_links("AppCore", "Ultralight", "UltralightCore", "WebCore")
    -- Delay-load the Ultralight DLLs: the framework DLL must still LOAD before the package preflight
    -- runs. With delay-load the imports resolve only on first
    -- use of the accelerated backend -- which does not happen until it is registered -- so an in-game
    -- load without the DLLs present cannot fail here. add_shflags targets the shared-library (DLL)
    -- linker; add_ldflags would only hit executables.
    add_shflags("/DELAYLOAD:UltralightCore.dll", "/DELAYLOAD:WebCore.dll",
                "/DELAYLOAD:Ultralight.dll", "/DELAYLOAD:AppCore.dll", { force = true })
    add_links("delayimp")
    add_files("src/**.cpp", "src/resource.rc")
    -- src/VR/** links CommonLibF4VR and belongs only to PrismaUI_F4VR.
    remove_files("src/VR/**.cpp")
    set_pcxxheader("src/PCH.h")

    -- Packages
    add_packages("minhook", "spdlog")
    if not os.getenv("PRISMA_DIRECTXTK_ROOT") then
        add_packages("directxtk")
    end

    -- psapi: GetProcessMemoryInfo for ModelPreview's memory telemetry.
    add_syslinks("comctl32", "shell32", "imm32", "bcrypt", "version", "d3dcompiler", "psapi", "dbghelp", "dxguid")

    -- Preprocessor
    add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX", "_UNICODE")

    if is_plat("windows") then
        -- Conformance + suppress known-safe warnings from CommonLib headers
        add_cxflags("/permissive-", "/wd4200", "/wd4201", "/wd4324")
        add_cxflags("/Gy", "/Gw", "/Zc:inline", "/d2ReducedOptimizeHugeFunctions")
        -- Async SEH exceptions — required for _set_se_translator.
        -- /FS is deliberately absent: it forces every parallel cl.exe to serialise its PDB
        -- writes through the out-of-process mspdbsrv.exe. We build with /Z7 (below), which
        -- writes debug info straight into the .obj and never opens a PDB during compile,
        -- so there is nothing for /FS to serialise.
        add_cxflags("/EHa", "/bigobj")
        -- Suppress the D9025 "overriding /EHs with /EHa" warning from xmake's default flags.
        add_cxflags("/wd4711")
        -- Warnings ON for framework-owned source. This was build.warning=false, which hid EVERY
        -- compiler diagnostic on the shipping DLL -- for code this dependent on ABI layouts, casts,
        -- threads and hooks, that is exactly where a warning earns its keep. /W3 ("more") and NOT
        -- warnings-as-errors, so a dependency-header diagnostic surfaces without breaking the build;
        -- scope dependency noise with /external:W0 as a follow-up if it proves loud in CI.
        set_warnings("more")
        -- Debug info in release.
        --
        -- /Z7, not /Zi. /Zi writes to a shared .pdb via mspdbsrv.exe, an out-of-process
        -- server cl.exe talks to over named pipes + shared memory. That handshake is not
        -- reliable under Wine: it fails with
        --     fatal error C1902: Program database manager mismatch
        -- on every translation unit, before any source is read. Reproduced on a clean
        -- wineprefix with a single MSVC install (14.44.35207), so it is not an actual
        -- version mismatch -- /Zi fails and /Z7 succeeds with everything else held equal.
        --
        -- /Z7 embeds CodeView debug info directly in each .obj. The /DEBUG link below
        -- still emits a complete PDB from it, so the shipped artifact is unchanged.
        add_cxflags("/Z7", { release = true })
        add_ldflags("/DEBUG", "/OPT:REF", "/OPT:ICF", { release = true, force = true })
    end

    after_build(function(target)
        local proj     = os.scriptdir()
        local ver      = PRISMA_VERSION
        local game_ver = os.getenv("PRISMA_TARGET") or "flat"
        local distdir  = path.join(proj, "dist", "PrismaUI_F4_" .. ver .. "_" .. string.upper(game_ver))

        -- DLL
        local f4se_plugins = path.join(distdir, "F4SE", "plugins")
        os.mkdir(f4se_plugins)
        os.cp(target:targetfile(), f4se_plugins)

        -- Assets (views, translations, etc.)
        -- os.cp of a directory copies its CONTENTS when the destination is absent, but copies the
        -- directory ITSELF inside when the destination already exists -- so a rebuild without the
        -- tryrm yields PrismaUI_F4/assets/views, then PrismaUI_F4/assets/assets/views. Same reason
        -- the shell copy below clears first.
        local assets_src = path.join(proj, "assets")
        if os.isdir(assets_src) then
            local assets_dest = path.join(distdir, "PrismaUI_F4")
            os.tryrm(assets_dest)
            os.cp(assets_src, assets_dest)
            local inspector = path.join(assets_dest, "views", "inspector")
            if os.isdir(inspector) then
                local protectedInspector = path.join(assets_dest, "inspector")
                os.tryrm(protectedInspector)
                os.cp(inspector, protectedInspector)
                os.tryrm(inspector)
            end
        end

        -- Framework Papyrus native declarations ship with the framework. This keeps on-mesh
        -- The folder on disk is lowercase "scripts"; Windows matched "Scripts" anyway and Linux
        -- silently did not, dropping PrismaUI.pex from the package.
        local scripts_src = path.join(proj, "Scripts")
        if not os.isdir(scripts_src) then
            scripts_src = path.join(proj, "scripts")
        end
        if os.isdir(scripts_src) then
            local scripts_dest = path.join(distdir, "Scripts")
            os.tryrm(scripts_dest)
            os.cp(scripts_src, scripts_dest)
        end

        -- Best-effort local convenience only; a compile-only CI runner may lack Python/7-Zip/the SDK.
        local find_tool = import("lib.detect.find_tool", {anonymous = true})
        local python_cmd = {os.host() == "windows" and "python" or "python3"}
        for _, cand in ipairs({{"python3"}, {"python"}, {"py", "-3"}}) do
            local tool = find_tool(cand[1])
            if tool then
                python_cmd = {tool.program}
                for i = 2, #cand do table.insert(python_cmd, cand[i]) end
                break
            end
        end
        local runtime_script = path.join(proj, "scripts", "stage-ultralight-runtime.py")
        local runtime_staged = false
        try
        {
            function ()
                local runtime_args = {runtime_script, "--package-root", path.join(distdir, "PrismaUI_F4")}
                local runtime_source = os.getenv("ULTRALIGHT_RUNTIME_DIR")
                if runtime_source then
                    table.insert(runtime_args, "--source")
                    table.insert(runtime_args, runtime_source)
                end
                os.vrunv(python_cmd[1], table.join(table.slice(python_cmd, 2), runtime_args))
                local manifest_dest = path.join(distdir, "PrismaUI_F4", "runtime")
                os.mkdir(manifest_dest)
                os.cp(path.join(proj, "runtime", "ultralight-1.4.0.manifest.json"), manifest_dest)
                runtime_staged = true
            end,
            catch
            {
                function (errors)
                    print("Skipped Ultralight runtime staging (compile-only build?): " .. tostring(errors))
                end
            }
        }

        print("Distribution ready: " .. distdir)

        local mods_path = os.getenv("XSE_FO4_MODS_PATH")
        if mods_path then
            if not runtime_staged then
                print("Not deploying: pinned Ultralight runtime was not staged (set ULTRALIGHT_RUNTIME_DIR).")
            else
                -- --deploy-root swaps only F4SE/PrismaUI_F4/Scripts, preserving MO2 meta.ini.
                local deploy_args = {runtime_script, "--deploy-from", distdir,
                    "--deploy-root", path.join(mods_path, "PrismaUI_F4")}
                os.vrunv(python_cmd[1], table.join(table.slice(python_cmd, 2), deploy_args))
                print("Deployed validated PrismaUI_F4 package to " .. path.join(mods_path, "PrismaUI_F4"))
            end
        end
    end)

target("PrismaUI-F4-Example-Plugin")
    set_default(false)
    set_kind("shared")
    set_languages("c++23")
    set_filename("PrismaUI-F4-Example.dll")

    add_rules("commonlibf4.plugin", {
        name    = "PrismaUI-F4-Example-Plugin",
        author  = "PrismaUI",
        version = "1.0.0"
    })

    add_includedirs("example-f4se-plugin/src")
    add_files("example-f4se-plugin/src/**.cpp")
    set_pcxxheader("example-f4se-plugin/src/PCH.h")

    add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX")

    if is_plat("windows") then
        add_cxflags("/permissive-", "/wd4200", "/wd4201", "/wd4324")
        add_syslinks("Version", "Ole32", "OleAut32", "User32", "bcrypt", "crypt32")
    end

    after_build(function(target)
        print("[PrismaUI-F4-Example] built to: " .. target:targetfile())
    end)

includes("xmake/tests.lua")

-- ---------------------------------------------------------------------------------------------
-- ModelPreview Interface3D prototype -- compile-only
--
-- src/PrismaUI/ModelPreviewInterface3D.cpp is dead code behind PRISMA_MODELPREVIEW_INTERFACE3D_PROTOTYPE,
-- which NO target defines. The flat target globs src/**.cpp, so the file is compiled everywhere as an
-- EMPTY translation unit: nothing inside its `#if` has ever been compiled, and every green DLL build
-- says nothing about it. That is how `RE::Interface3D::UI_DEPTH_PRIORITY` survived in the file -- the
-- enum is `RE::UI_DEPTH_PRIORITY`, so the qualified name does not exist and the body cannot compile.
--
-- This target exists only to compile that body. It is compile-only (never linked into a provider),
-- not built by default, and built explicitly by the flat framework job
-- (scripts/build-windows-framework.ps1). Cost is one translation unit in a job that already compiles
-- the same file, and it is the only thing that turns "the prototype rotted" into a red build.
target("PrismaModelPreviewPrototype")
    set_kind("object")
    set_languages("c++23")
    set_default(false)
    set_group("tests")

    -- Same CommonLib the flat provider uses: the prototype calls RE::Interface3D::Renderer and
    -- RE::BSGraphics::GetRendererData, both of which the flat CommonLibF4 declares.
    add_deps("commonlibf4")
    -- F4SE/Impl/PCH.h includes <spdlog/spdlog.h>.
    add_packages("spdlog")

    add_includedirs("src", "external/ultralight-1.4.0/include")
    add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX", "UNICODE", "_UNICODE",
                "PRISMA_MODELPREVIEW_INTERFACE3D_PROTOTYPE")
    add_files("src/PrismaUI/ModelPreviewInterface3D.cpp")

    if is_plat("windows") then
        add_cxflags("/permissive-", "/EHa", "/bigobj")
        -- Same CommonLib header noise the flat target suppresses.
        add_cxflags("/wd4200", "/wd4201", "/wd4324")
    end

-- ---------------------------------------------------------------------------------------------
-- Fallout 4 VR
--
-- A separate provider DLL, not a fourth runtime of the flat one: FO4VR links CommonLibF4VR (a
-- different CommonLib fork with the RE/Bethesda/ header layout) and the flat CommonLibF4's
--
-- ---------------------------------------------------------------------------------------------

-- Deterministic coverage for the parts of the VR port that are pure math/policy: spatial
-- geometry, pointer routing, resource budgets, render-target policy, thread migration. No
-- CommonLib, no D3D device, no game -- so it runs anywhere the toolchain does.
target("PrismaUI_F4VR_Tests")
    set_kind("binary")
    set_filename("PrismaUI_F4VR_Tests.exe")
    set_languages("c++23")
    set_default(false)
    set_group("tests")

    add_includedirs("src")
    add_files(
        "tests/vr/DeterministicTests.cpp",
        "tests/vr/DeterministicWorldPanelTests.cpp",
        "tests/vr/DeterministicPointerTests.cpp",
        "tests/vr/DeterministicInputRegionTests.cpp",
        "src/PrismaUI/InputRegionPolicy.cpp",
        "src/VR/WorldPanelGeometry.cpp")
    add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX", "UNICODE", "_UNICODE")

    if is_plat("windows") then
        add_cxflags("/permissive-", "/Zc:preprocessor")
        add_syslinks("user32")
    end

-- The FO4VR provider DLL, behind PRISMA_TARGET=vr.
--
-- It is opt-in rather than always-configured because CommonLibF4VR's fallout_f4/fallout_f4ng/
-- fallout_f4vr options are GLOBAL and all default on, and with more than one of them on, the
-- structs whose layout differs between flat and VR (BSGeometry among them) expose no members at
-- all -- the header only emits them for an unambiguous single-runtime build. The VR graph
-- therefore has to own the whole configuration:
--
--   PRISMA_TARGET=vr xmake f --fallout_f4=n --fallout_f4ng=n --fallout_f4vr=y
--   PRISMA_TARGET=vr xmake build PrismaUI_F4VR
--
-- A normal OG/NG checkout never evaluates any of this and configures exactly as before.
local PRISMA_TARGET = string.lower(os.getenv("PRISMA_TARGET") or "")
local F4VR_ROOT = path.join(os.scriptdir(), "lib", "commonlibf4vr")
if PRISMA_TARGET == "vr" and os.isfile(path.join(F4VR_ROOT, "xmake.lua")) then
    includes("lib/commonlibf4vr")

target("PrismaUI_F4VR")
    set_kind("shared")
    set_filename("PrismaUI_F4VR.dll")
    set_symbols("debug")
    set_default(false)

    -- CommonLibF4VR's own xmake project names its library target commonlibf4-ng.
    add_deps("commonlibf4-ng")

    on_config(function()
        if not has_config("fallout_f4vr") or
            has_config("fallout_f4") or
            has_config("fallout_f4ng") then
            os.raise(
                "The VR graph must be configured with " ..
                "--fallout_f4=n --fallout_f4ng=n --fallout_f4vr=y -- a multi-runtime " ..
                "CommonLibF4VR build hides the members whose layout differs in VR")
        end
    end)

    add_includedirs("src")
    add_includedirs("external/ultralight-1.4.0/include")
    add_linkdirs("external/ultralight-1.4.0/lib")
    add_links("AppCore", "Ultralight", "UltralightCore", "WebCore", "delayimp")
    add_shflags("/DELAYLOAD:UltralightCore.dll", "/DELAYLOAD:WebCore.dll",
                "/DELAYLOAD:Ultralight.dll", "/DELAYLOAD:AppCore.dll", { force = true })
    add_forceincludes("VRCompat.h")

    add_files(
        "src/VR/**.cpp",
        "src/API/**.cpp",
        "src/Menus/**.cpp",
        "src/Utils/**.cpp",
        "src/Hooks/InlineHookClassifier.cpp",
        -- The WebRuntime implementation was split out of WebRuntime.cpp's text-included .inl
        -- fragments into real translation units; the flat target picks them up via its glob, this
        -- explicit list must name every one of them.
        "src/PrismaUI/WebRuntime.cpp",
        "src/PrismaUI/WebRuntimeState.cpp",
        "src/PrismaUI/WebRuntimeLifecycle.cpp",
        "src/PrismaUI/WebRuntimeFrame.cpp",
        "src/PrismaUI/WebRuntimePresent.cpp",
        "src/PrismaUI/WebRuntimePresentation.cpp",
        "src/PrismaUI/WebRuntimeFocus.cpp",
        "src/PrismaUI/WebRuntimeInput.cpp",
        "src/PrismaUI/WebRuntimeViews.cpp",
        "src/PrismaUI/WebRuntimeInspector.cpp",
        "src/PrismaUI/WebRuntimeCallbacks.cpp",
        "src/PrismaUI/DockBridge.cpp",
        "src/PrismaUI/UltralightBackend.cpp",
        "src/PrismaUI/UltralightBackendCallbacks.cpp",
        "src/PrismaUI/UltralightBackendDispatch.cpp",
        "src/PrismaUI/UltralightDispatchQueue.cpp",
        "src/PrismaUI/UltralightBackendFrames.cpp",
        "src/PrismaUI/UltralightBackendInput.cpp",
        "src/PrismaUI/UltralightBackendPresentation.cpp",
        "src/PrismaUI/UltralightBackendViewState.cpp",
        "src/PrismaUI/UltralightBackendViews.cpp",
        "src/PrismaUI/UltralightViewCallbacks.cpp",
        "src/PrismaUI/UltralightViewManager.cpp",
        "src/PrismaUI/UltralightDocumentCallbacks.cpp",
        "src/PrismaUI/UltralightImageSources.cpp",
        "src/PrismaUI/UltralightRuntime.cpp",
        "src/PrismaUI/PrismaFileSystem.cpp",
        "src/PrismaUI/SystemClipboard.cpp",
        "src/PrismaUI/UltralightInspector.cpp",
        "src/PrismaUI/FreezeDiagnostics.cpp",
        "src/PrismaUI/FreezeDiagnosticsCore.cpp",
        "src/GPU/**.cpp",
        -- WebRuntime.cpp's offscreen (on-mesh) frame tracking lives here since the pass-3 split.
        -- The flat target picks it up via its own glob; this explicit list does not.
        "src/PrismaUI/OffscreenFrames.cpp",
        -- WebRuntime.cpp calls the present profiler, so the VR target must link it too (the flat
        -- framework target picks it up via its own glob; this explicit list does not).
        "src/PrismaUI/PresentProfiler.cpp",
        "src/PrismaUI/HwndThreadBootstrap.cpp",
        "src/PrismaUI/WebInput.cpp",
        "src/PrismaUI/WebInputWndProc.cpp",
        "src/PrismaUI/ControllerGlyphs.cpp",
        "src/PrismaUI/InputRegionPolicy.cpp",
        "src/PrismaUI/InputMask.cpp",
        "src/PrismaUI/PapyrusBridge.cpp",
        "src/PrismaUI/PapyrusVM.cpp",
        "src/PrismaUI/Translations.cpp",
        "src/PrismaUI/TranslationsScript.cpp",
        "src/PrismaUI/TranslationsText.cpp",
        "src/PrismaUI/TranslationsJson.cpp",
        "src/PrismaUI/TranslationsLanguage.cpp",
        "src/resource.rc")
    remove_files("src/API/GameThreadAPI.cpp")
    -- Deliberately absent, and NOT oversights:
    --   main.cpp                 -- VRMain.cpp carries the F4SEVR Query/Load contract instead
    --                              WebRuntime.cpp is in this target and calls it.
    --   GameThreadDispatcher.cpp -- flat HWND dispatcher. VR links the stubs for Dispatch,
    --                              DispatchSafety, DropView, DetachWindow and HandleWindowMessage
    --                              from src/VR/VRUnportedStubs.cpp. AttachWindow, RecoverWindow and
    --                              IsReady stay unstubbed, so their callers stay inside #ifndef PRISMAUI_FO4VR.
    --   VirtualPointer.cpp / MenuCursorDiagnostics.cpp -- flat-only; VR links their stubs from
    --                              src/VR/VRUnportedStubs.cpp.
    --   WebCompositor.cpp        -- flat-only; ToString lives in the header.
    --   ModelPreviewInterface3D.cpp -- only built by the PrismaModelPreviewPrototype target.
    --   ModelPreview / LocalMapSpike / CameraSpike / ActorPreviewSpike / TextureOverlay /
    --   VanillaUISuppressor      -- all hook 1.10.163-exact OG addresses or OG vtable RVAs.
    --                            ActorPreviewSpike additionally needs Interface3D, which VR
    --                            has no screen system for at all.
    set_pcxxheader("src/PCH.h")

    add_packages("minhook", "spdlog")
    if not os.getenv("PRISMA_DIRECTXTK_ROOT") then
        add_packages("directxtk")
    end
    add_syslinks("comctl32", "shell32", "imm32", "bcrypt", "version", "d3d11", "dxgi", "d3dcompiler", "psapi", "dbghelp", "dxguid")

    add_defines(
        "WIN32_LEAN_AND_MEAN",
        "NOMINMAX",
        "_UNICODE",
        "PRISMAUI_FO4VR=1")

    if is_plat("windows") then
        add_cxflags("/permissive-", "/wd4200", "/wd4201", "/wd4324")
        add_cxflags("/Gy", "/Gw", "/Zc:inline", "/d2ReducedOptimizeHugeFunctions")
        -- /FS dropped and /Z7 in place of /Zi: see the PrismaUI_F4 target above. /Zi routes
        -- PDB writes through mspdbsrv.exe, which fails under Wine with C1902.
        add_cxflags("/EHa", "/bigobj", "/wd4711")
        -- Warnings ON for framework-owned source; see the PrismaUI_F4 target. /W3, not errors.
        set_warnings("more")
        add_cxflags("/Z7", { release = true })
        add_ldflags("/DEBUG", "/OPT:REF", "/OPT:ICF", { release = true, force = true })
    end

    after_build(function(target)
        print("[PrismaUI_F4VR] built to: " .. target:targetfile())
    end)
end


target("PrismaUI_PapyrusJsonTests")
    set_kind("binary")
    set_languages("c++23")
    set_filename("PrismaUI_PapyrusJsonTests.exe")
    add_includedirs("src")
    add_files("tests/papyrus_json_tests.cpp")

target("PrismaUI_FreezeDiagnosticsTests")
    set_kind("binary")
    set_filename("PrismaUI_FreezeDiagnosticsTests.exe")
    set_languages("c++23")
    set_default(false)
    set_group("tests")
    add_includedirs("src")
    add_files("tests/freeze_diagnostics_test.cpp", "src/PrismaUI/FreezeDiagnosticsCore.cpp")
