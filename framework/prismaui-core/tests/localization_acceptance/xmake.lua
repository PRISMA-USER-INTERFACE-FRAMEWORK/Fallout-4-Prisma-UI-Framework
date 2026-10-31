target("PrismaLocalizationAcceptance")
    set_default(false)
    set_kind("shared")
    set_arch("x64")
    set_filename("PrismaLocalizationAcceptance.dll")
    set_version("1.0.0")
    add_rules("commonlibf4.plugin", {
        name = "PrismaLocalizationAcceptance",
        author = "PrismaUI",
        version = "1.0.0",
    })
    add_deps("commonlibf4")
    add_packages("spdlog")
    add_files("src/**.cpp")
    add_includedirs("src", path.join(os.scriptdir(), "../../src"))
    set_pcxxheader("src/PCH.h")
    add_defines("COMMONLIB_RUNTIMECOUNT=3", "WIN32_LEAN_AND_MEAN", "NOMINMAX")
    add_syslinks("bcrypt", "version", "user32")
    if is_plat("windows") then
        add_cxflags("/permissive-", "/wd4200", "/wd4201", "/wd4324", "/EHa", "/bigobj")
    end
    set_targetdir("$(builddir)/windows/x64/release")

if string.lower(os.getenv("PRISMA_TARGET") or "") == "vr" and
   os.isfile(path.join(os.scriptdir(), "../../lib/commonlibf4vr/xmake.lua")) then
    target("PrismaLocalizationAcceptanceVR")
        set_default(false)
        set_kind("shared")
        set_arch("x64")
        set_filename("PrismaLocalizationAcceptanceVR.dll")
        set_version("1.0.0")
        add_deps("commonlibf4-ng")
        add_packages("spdlog")
        add_files("src/main.cpp")
        add_includedirs("src", path.join(os.scriptdir(), "../../src"))
        set_pcxxheader("src/PCH.h")
        add_defines("PRISMA_LOCALIZATION_ACCEPTANCE_VR=1", "COMMONLIB_RUNTIMECOUNT=3", "WIN32_LEAN_AND_MEAN", "NOMINMAX")
        add_syslinks("bcrypt", "version", "user32")
        if is_plat("windows") then
            add_cxflags("/permissive-", "/wd4200", "/wd4201", "/wd4324", "/EHa", "/bigobj")
        end
        set_targetdir("$(builddir)/windows/x64/release")
end
