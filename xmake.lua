set_xmakever("3.0.0")
set_project("Prisma-Matrix")
set_languages("c++23")

local components = {
    "shared",
    "framework",
    "tools"
}

for _, component in ipairs(components) do
    local script = path.join(os.scriptdir(), component, "xmake.lua")
    if os.isfile(script) then
        includes(component)
    end
end
