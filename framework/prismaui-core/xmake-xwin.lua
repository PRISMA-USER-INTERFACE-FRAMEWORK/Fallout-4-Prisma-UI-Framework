includes("toolchains/xwin-clang-cl.lua")
set_toolchains("xwin-clang-cl")
set_runtimes("MD")

-- xwin's ucrt stddef.h defines offsetof via reinterpret_cast, which is not a
-- constant expression, so CommonLib's layout static_asserts fail without this.
add_defines("_CRT_USE_BUILTIN_OFFSETOF")

-- REX::W32 exists so CommonLibF4 never includes windows.h. Anything that drags
-- windows.h in re-introduces its MAX_PATH and ERROR macros, which then collide
-- with REX::W32::MAX_PATH and REX::ERROR.
add_defines("NOGDI", "NOMINMAX", "WIN32_LEAN_AND_MEAN")

-- spdlog is the usual vector for that: F4SE/API.cpp includes
-- <spdlog/sinks/basic_file_sink.h>, and a header-only spdlog pulls its *-inl.h,
-- which includes windows.h. SPDLOG_COMPILED_LIB keeps the implementation out of
-- every translation unit; the symbols come from the prebuilt library below.
add_defines("SPDLOG_COMPILED_LIB", "SPDLOG_USE_STD_FORMAT", "SPDLOG_WCHAR_TO_UTF8_SUPPORT")

add_requireconfs("**.cmake", {system = true, override = true})

add_requireconfs("spdlog", {configs = {header_only = true, wchar = true, std_format = true}, override = true})

-- The staged Windows packages live under /opt on the prisma-light runner.
local pkgroot = os.getenv("PRISMA_WIN_PACKAGES")
if not pkgroot then
    local toolchain_root = os.getenv("PRISMA_WIN_TOOLCHAIN")
    if toolchain_root then
        pkgroot = path.join(toolchain_root, "Users/Game/AppData/Local/.xmake/packages")
    else
        pkgroot = "/opt/prisma-toolchain/win-packages"
    end
end

-- xmake's --file description scope does not expose a normal error()/raise()
-- helper in the pinned version. Print the useful reason and deliberately call
-- an undefined sentinel to abort configuration.
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

local directxtk_root = os.getenv("PRISMA_DIRECTXTK_ROOT")
if not directxtk_root or not os.isfile(path.join(directxtk_root, "lib", "DirectXTK.lib")) then
    die("PRISMA_DIRECTXTK_ROOT must contain the verified clang-cl DirectXTK build")
end
add_includedirs(directxtk_root)

add_linkdirs(staged_lib("s", "spdlog", "spdlog"))
add_linkdirs(path.join(directxtk_root, "lib"))
add_links("spdlog", "DirectXTK")

includes("xmake.lua")
