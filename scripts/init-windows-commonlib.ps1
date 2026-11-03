$ErrorActionPreference = 'Stop'

$workspace = $env:GITHUB_WORKSPACE
foreach ($name in @('shared/CommonLibF4', 'framework/prismaui-core/lib/commonlibf4vr')) {
    $submodulePath = Join-Path $env:GITHUB_WORKSPACE ($name -replace '/', '\')
    $gitlink = ((& git -c "safe.directory=$workspace" ls-tree HEAD -- $name) | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($gitlink)) {
        throw "The $name gitlink is missing from HEAD"
    }

    $parts = $gitlink -split '\s+'
    $commit = $parts[2]
    if ([string]::IsNullOrWhiteSpace($commit)) {
        throw "The $name gitlink does not contain a commit"
    }

    $configName = "submodule.$name.url"
    $url = ((& git -c "safe.directory=$workspace" config -f .gitmodules --get $configName) | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($url)) {
        throw "The $name URL is missing from .gitmodules"
    }

    if (Test-Path -LiteralPath $submodulePath) {
        Remove-Item -LiteralPath $submodulePath -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path (Split-Path $submodulePath) | Out-Null
    New-Item -ItemType Directory -Force -Path $submodulePath | Out-Null

    & git -c "safe.directory=$submodulePath" -C $submodulePath init --quiet
    if ($LASTEXITCODE -ne 0) { throw "$name repository initialization failed" }
    & git -c "safe.directory=$submodulePath" -C $submodulePath remote add origin $url
    if ($LASTEXITCODE -ne 0) { throw "$name remote setup failed" }
    & git -c "safe.directory=$submodulePath" -C $submodulePath fetch --depth=1 origin $commit
    if ($LASTEXITCODE -ne 0) { throw "Unable to fetch $name commit $commit" }

    & git -c "safe.directory=$submodulePath" -C $submodulePath checkout --detach $commit
    if ($LASTEXITCODE -ne 0) { throw "Unable to check out $name commit $commit" }

    & git -c "safe.directory=$submodulePath" -C $submodulePath submodule update --init --recursive
    if ($LASTEXITCODE -ne 0) { throw "$name nested submodule initialization failed" }

    Write-Host "Initialized $name at $commit"
}
