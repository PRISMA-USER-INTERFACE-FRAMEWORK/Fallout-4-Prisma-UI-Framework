set_xmakever("3.0.0")
set_project("PrismaControllerTest_F4")
set_languages("c++23")

includes("../../shared")

local plugin_dir = os.scriptdir()

target("PrismaControllerTest_F4")
    set_kind("shared")
    set_arch("x64")
    set_filename("PrismaControllerTest_F4.dll")
    set_version("1.0.0")

    add_deps("commonlibf4")
    add_rules("commonlibf4.plugin", {
        name = "PrismaControllerTest_F4",
        author = "PrismaUI",
        version = "1.0.0"
    })

    add_includedirs("src", "../../framework/prismaui-core/src")
    add_files("src/**.cpp")
    set_pcxxheader("src/PCH.h")
    add_defines("COMMONLIB_RUNTIMECOUNT=3", "WIN32_LEAN_AND_MEAN", "NOMINMAX", "_UNICODE")
    add_syslinks("bcrypt", "version")

    if is_plat("windows") then
        add_cxflags("/permissive-", "/wd4200", "/wd4201", "/wd4324", "/bigobj")
        set_policy("build.warning", false)
    end

    after_build(function(target)
        local deploy_dir = os.getenv("DEPLOY_PATH")
        if not deploy_dir or deploy_dir == "" then return end

        local plugins = path.join(deploy_dir, "F4SE/Plugins")
        local views = path.join(deploy_dir, "PrismaUI_F4/views/Interface/PrismaControllerTest_F4")
        os.mkdir(plugins)
        os.mkdir(views)
        os.cp(target:targetfile(), path.join(plugins, "PrismaControllerTest_F4.dll"))
        for _, name in ipairs({ "controller_test.html", "controller_test.css", "controller_test.js" }) do
            os.cp(path.join(plugin_dir, "assets/views", name), path.join(views, name))
        end
        print("PrismaControllerTest_F4 deployed to " .. deploy_dir)
    end)
