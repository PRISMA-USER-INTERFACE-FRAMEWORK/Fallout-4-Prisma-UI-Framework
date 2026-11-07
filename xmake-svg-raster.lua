set_xmakever("3.0.0")
set_project("PrismaUI-SvgRasterAcceptance")
set_languages("c++23")

if is_host("linux") then
    includes("framework/prismaui-core/toolchains/xwin-clang-cl.lua")
    set_toolchains("xwin-clang-cl")
end
set_runtimes("MD")

local sdk = os.getenv("ULTRALIGHT_SDK_DIR") or "framework/prismaui-core/external/ultralight-1.4.0"
local include_dir = path.join(sdk, "include")
local lib_dir = path.join(sdk, "lib")

if not os.isdir(include_dir) then
    print("error: Ultralight include directory not found: " .. include_dir)
    ULTRALIGHT_SVG_RASTER_SDK_INCLUDE_NOT_FOUND()
end
if not os.isdir(lib_dir) then
    print("error: Ultralight library directory not found: " .. lib_dir)
    ULTRALIGHT_SVG_RASTER_SDK_LIB_NOT_FOUND()
end

target("PrismaUI_UltralightSvgRasterAcceptance")
    set_kind("binary")
    set_filename("PrismaUI_UltralightSvgRasterAcceptance.exe")
    add_includedirs(include_dir)
    add_linkdirs(lib_dir)
    add_links("AppCore", "Ultralight", "UltralightCore", "WebCore")
    add_files("framework/prismaui-core/tests/ultralight_svg_raster_acceptance.cpp")
    add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX")
