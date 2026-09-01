set_xmakever("3.0.0")
set_project("PrismaUI-F4-Example")
set_version("1.0.0")
set_languages("c++23")

add_rules("mode.release", "mode.releasedbg", "mode.debug")

-- CommonLibF4 submodule lives one level up at lib/commonlibf4
includes("../lib/commonlibf4")

target("PrismaUI-F4-Example-Plugin")
    set_kind("shared")
    set_languages("c++23")
    set_filename("PrismaUI-F4-Example.dll")

    add_rules("commonlibf4.plugin", {
        name    = "PrismaUI-F4-Example-Plugin",
        author  = "PrismaUI",
        version = "1.0.0"
    })

    add_includedirs("src")
    add_files("src/**.cpp")

    set_pcxxheader("src/PCH.h")

    add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX")

    if is_plat("windows") then
        add_cxflags("/permissive-", "/wd4200", "/wd4201", "/wd4324")
        add_syslinks("Version", "Ole32", "OleAut32", "User32", "bcrypt", "crypt32")
    end

    after_build(function(target)
        print("[PrismaUI-F4-Example] built to: " .. target:targetfile())
    end)
