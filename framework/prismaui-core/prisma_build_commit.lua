rule("prisma_build_commit")
    on_load(function(target)
        local commit = (os.getenv("GITHUB_SHA") or ""):sub(1, 8)
        if commit == "" then
            local out = try { function() return os.iorunv("git", {"rev-parse", "--short=8", "HEAD"}) end }
            commit = out and out:match("^%s*(%x+)") or "unknown"
        end
        target:add("defines", "PRISMA_BUILD_COMMIT=\"" .. commit .. "\"")
    end)
rule_end()
