-- Matrix-root xwin/clang-cl entry point for the FRAMEWORK compile gate.
--
-- Why this file exists (and is not framework/prismaui-core/xmake-xwin.lua):
--
-- The framework subtree's own xmake-xwin.lua ends in `includes("xmake.lua")`, i.e. it configures
-- the framework's build graph ALONE. That worked in the standalone PrismaUI_F4 repo, where the
-- framework carried its own `lib/commonlibf4` submodule and defined the `commonlibf4` target (and
-- therefore the `commonlibf4.plugin` rule) itself. Under Matrix that gitlink is gone on purpose:
-- CommonLibF4 is a SINGLE pinned submodule at `shared/CommonLibF4`, included by `shared/xmake.lua`
-- monorepo therefore dies at configure time with:
--
--     error: unknown rule(commonlibf4.plugin) in target(PrismaUI-F4-Example-Plugin)!
--
-- The fix is not to re-standalone-ise the subtree by copying a CommonLib source tree back into
-- `framework/prismaui-core/lib/commonlibf4` -- it is to configure the framework through Matrix's
-- own build graph, shared/ first, exactly as the root xmake.lua orders it.
--
-- xmake resolves every add_requires() in the included graph eagerly at `xmake f` time, so pulling
-- them in would cost package resolution for targets this gate never builds.

set_xmakever("3.0.0")
set_project("Prisma-Matrix")
-- PrismaUI_F4 does not set_languages() itself; it inherits c++23 from the root xmake.lua, which
-- this adapter replaces rather than includes.
set_languages("c++23")

includes("framework/prismaui-core/toolchains/xwin-clang-cl.lua")
set_toolchains("xwin-clang-cl")
set_runtimes("MD")

-- xwin's ucrt stddef.h defines offsetof via reinterpret_cast, which is not a constant expression,
-- so CommonLib's layout static_asserts fail without this.
add_defines("_CRT_USE_BUILTIN_OFFSETOF")

-- REX::W32 exists so CommonLibF4 never includes windows.h. Anything that drags windows.h in
-- re-introduces its MAX_PATH and ERROR macros, which then collide with REX::W32::MAX_PATH and
-- REX::ERROR.
add_defines("NOGDI", "NOMINMAX", "WIN32_LEAN_AND_MEAN")

-- spdlog is the usual vector for that: F4SE/API.cpp includes <spdlog/sinks/basic_file_sink.h>,
-- and a header-only spdlog pulls its *-inl.h, which includes windows.h. SPDLOG_COMPILED_LIB keeps
-- the implementation out of every translation unit; the symbols come from the staged library
-- linked below instead.
add_defines("SPDLOG_COMPILED_LIB", "SPDLOG_USE_STD_FORMAT", "SPDLOG_WCHAR_TO_UTF8_SUPPORT")

add_requireconfs("**.cmake", {system = true, override = true})

local native_windows = is_host("windows")
if native_windows then
    print("using native Windows clang-cl package resolution")
else
    -- xmake applies the project toolchain to targets but not to its own package manager, which
    -- goes looking for MSVC via vswhere and fails with "vs not found!" on cross-build hosts.
    add_requireconfs("spdlog", {configs = {header_only = true, wchar = true, std_format = true}, override = true})
    add_requireconfs("fmt", {configs = {header_only = true}, override = true})

    local pkgroot = os.getenv("PRISMA_WIN_PACKAGES")
    if not pkgroot then
        local toolchain_root = os.getenv("PRISMA_WIN_TOOLCHAIN")
        if toolchain_root then
            pkgroot = path.join(toolchain_root, "Users/Game/AppData/Local/.xmake/packages")
        else
            pkgroot = "/opt/prisma-toolchain/win-packages"
        end
    end

    local function die(msg)
        print("error: " .. msg)
        return STAGED_WINDOWS_TOOLCHAIN_NOT_FOUND()
    end

    local function staged_lib(letter, name, libname)
        for _, dir in ipairs(os.dirs(path.join(pkgroot, letter, name, "*", "*", "lib"))) do
            if os.isfile(path.join(dir, libname .. ".lib")) then
                return dir
            end
        end
        die("staged " .. libname .. ".lib not found under " .. pkgroot ..
            " -- set PRISMA_WIN_PACKAGES or PRISMA_WIN_TOOLCHAIN")
    end

    add_linkdirs(staged_lib("s", "spdlog", "spdlog"))
    add_links("spdlog")
end

local directxtk_root = os.getenv("PRISMA_DIRECTXTK_ROOT")
if not directxtk_root or not os.isfile(path.join(directxtk_root, "lib", "DirectXTK.lib")) then
    print("error: PRISMA_DIRECTXTK_ROOT must contain the verified clang-cl DirectXTK build")
    PRISMA_DIRECTXTK_ROOT_MISSING()
end
add_includedirs(directxtk_root)
add_linkdirs(path.join(directxtk_root, "lib"))
add_links("DirectXTK")

-- shared/ FIRST: it declares the pinned `commonlibf4` target and the `commonlibf4.plugin` rule
-- that framework/prismaui-core's targets consume. Same ordering contract as the root xmake.lua.
includes("shared")
includes("framework")
