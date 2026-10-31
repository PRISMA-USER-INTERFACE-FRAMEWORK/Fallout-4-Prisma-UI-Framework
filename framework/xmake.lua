-- Component build files remain under their own directory and are included by the root
-- when present (see docs/ARCHITECTURE.md's "Build direction"). prismaui-core/xmake.lua
-- defines the framework's targets; see that file for the CommonLibF4 build adapter note
-- (it consumes shared/xmake.lua's pinned commonlibf4 target rather than a second one).
includes("prismaui-core")

if os.getenv("PRISMA_LOCALIZATION_ACCEPTANCE") == "1" then
    includes("prismaui-core/tests/localization_acceptance")
end

if os.getenv("PRISMA_CONTROLLER_TEST") == "1" then
    includes("../tools/controller-test")
end
