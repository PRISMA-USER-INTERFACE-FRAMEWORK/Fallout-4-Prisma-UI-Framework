param(
    [ValidateSet('PrismaUI_F4', 'PrismaUI_F4VR')]
    [string] $Target = 'PrismaUI_F4',
    [switch] $AcceleratedAcceptance,
    [switch] $Diagnostics,
    [switch] $LocalizationAcceptance,
    [switch] $ControllerTest,
    [string] $ExportRoot = ''
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ensure-windows-junction.ps1')
$workspace = [System.IO.Path]::GetFullPath($env:GITHUB_WORKSPACE).TrimEnd('\')
& (Join-Path $PSScriptRoot 'build-directxtk.ps1')
$workspaceParent = Split-Path $workspace -Parent
$cacheRoot = if ($env:PRISMA_WIN_BUILD_CACHE) {
    [System.IO.Path]::GetFullPath($env:PRISMA_WIN_BUILD_CACHE).TrimEnd('\')
} else {
    Join-Path $workspaceParent 'prisma-matrix-build-cache'
}

$variant = $Target.ToLowerInvariant()
$env:PRISMA_TARGET = if ($Target -eq 'PrismaUI_F4VR') { 'vr' } else { '' }
$env:PRISMA_LOCALIZATION_ACCEPTANCE = if ($LocalizationAcceptance) { '1' } else { '' }
$env:PRISMA_CONTROLLER_TEST = if ($ControllerTest) { '1' } else { '' }
if ($AcceleratedAcceptance) { $variant += '-accelerated' }
if ($Diagnostics) { $variant += '-diagnostics' }
if ($LocalizationAcceptance) {
    if ($Target -ne 'PrismaUI_F4') { throw 'LocalizationAcceptance is only produced for the flat target' }
    $variant += '-localization'
}
if ($ControllerTest -and $Target -ne 'PrismaUI_F4') { throw 'ControllerTest is only produced for the flat target' }

$inputFiles = @(
    'xmake.lua',
    'xmake-xwin-framework.lua',
    'framework/xmake.lua',
    'framework/prismaui-core/xmake.lua',
    'framework/prismaui-core/toolchains/xwin-clang-cl.lua',
    'scripts/build-directxtk.ps1',
    'scripts/verify-directxtk.ps1',
    'framework/prismaui-core/tests/localization_acceptance/xmake.lua',
    'framework/prismaui-core/tests/localization_acceptance/src/main.cpp',
    'tools/controller-test/xmake.lua'
)
$fingerprintInputs = foreach ($file in $inputFiles) {
    $path = Join-Path $workspace ($file -replace '/', '\')
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "fingerprint input is missing: $file" }
    "$file=$((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash)"
}
$gitlinks = @('shared/CommonLibF4', 'framework/prismaui-core/lib/commonlibf4vr')
foreach ($path in $gitlinks) {
    $line = ((& git -c "safe.directory=$workspace" ls-tree HEAD -- $path) | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($line)) { throw "fingerprint gitlink is missing: $path" }
    $fingerprintInputs += "$path=$($line -split '\s+')[2]"
}
$xmakeVersion = ((& xmake --version) | Out-String).Trim()
if ($LASTEXITCODE -ne 0) { throw 'unable to read xmake version' }

# Fingerprint the resolved compiler and linker, not just their version strings. Two LLVM
# installations can report the same version and still produce different binaries, so the
# installation path is part of the cache identity alongside the version.
function Resolve-PinnedLlmTool {
    param([Parameter(Mandatory = $true)][string] $Name)
    $llvmBin = if ($env:PRISMA_LLVM_BIN) { $env:PRISMA_LLVM_BIN } else { 'C:\Program Files\LLVM\bin' }
    $candidate = Join-Path $llvmBin $Name
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        throw "the pinned LLVM tool '$Name' was not found at $candidate; run scripts/setup-windows-clang-toolchain.ps1 or set PRISMA_LLVM_BIN"
    }
    return [System.IO.Path]::GetFullPath($candidate)
}

$clangPath = Resolve-PinnedLlmTool 'clang-cl.exe'
$clangVersion = ((& $clangPath --version) | Select-Object -First 1 | Out-String).Trim()
if ($LASTEXITCODE -ne 0) { throw "unable to read the version of $clangPath" }

$lldPath = Resolve-PinnedLlmTool 'lld-link.exe'
$lldVersion = ((& $lldPath --version) | Select-Object -First 1 | Out-String).Trim()
if ($LASTEXITCODE -ne 0) { throw "unable to read the version of $lldPath" }

$fingerprintInputs += "xmake=$xmakeVersion"
$fingerprintInputs += "clang-cl=$clangPath|$clangVersion"
$fingerprintInputs += "lld-link=$lldPath|$lldVersion"
$fingerprintInputs += "directxtk=$((Get-FileHash -LiteralPath (Join-Path $env:PRISMA_DIRECTXTK_ROOT 'lib\DirectXTK.lib') -Algorithm SHA256).Hash)"
$fingerprintInputs += "msvc=$($env:PRISMA_MSVC_VERSION)"
$fingerprintInputs += "windows-sdk=$($env:PRISMA_WINDOWS_SDK_VERSION)"
    $fingerprintInputs += "prisma-target=$($env:PRISMA_TARGET)"
    $fingerprintInputs += "localization-acceptance=$($env:PRISMA_LOCALIZATION_ACCEPTANCE)"
    $fingerprintInputs += "controller-test=$($env:PRISMA_CONTROLLER_TEST)"
$fingerprintInputs += "vr-cxxflags=$(if ($Target -eq 'PrismaUI_F4VR') { '-fdelayed-template-parsing' } else { '' })"
$fingerprintInputs += "variant=$variant"
$sourceRoots = @(
    (Join-Path $workspace 'framework/prismaui-core/src'),
    (Join-Path $workspace 'framework/prismaui-core/external'),
    (Join-Path $workspace 'shared/CommonLibF4/lib/commonlib-shared/src'),
    (Join-Path $workspace 'framework/prismaui-core/lib/commonlibf4vr/CommonLibF4/src')
)
$fingerprintExtensions = @('.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.hxx', '.rc', '.rc2', '.def', '.lib', '.manifest')
foreach ($sourceRoot in $sourceRoots) {
    if (Test-Path -LiteralPath $sourceRoot) {
        foreach ($source in Get-ChildItem -LiteralPath $sourceRoot -File -Recurse -Force |
                Where-Object { $_.Extension.ToLowerInvariant() -in $fingerprintExtensions }) {
            $relative = $source.FullName.Substring($workspace.Length).TrimStart('\')
            $fingerprintInputs += "source:$relative=$((Get-FileHash -LiteralPath $source.FullName -Algorithm SHA256).Hash)"
        }
    }
}
$fingerprintInputs += 'source-timestamp-policy=content-fingerprint-v2'
$sha = [System.Security.Cryptography.SHA256]::Create()
$fingerprint = (($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes(($fingerprintInputs -join "`n"))) |
        ForEach-Object { $_.ToString('x2') }) -join '').Substring(0, 20)
$buildRoot = Join-Path $cacheRoot ("framework-$variant-$fingerprint")
if ($buildRoot.StartsWith($workspace + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "persistent build cache must be outside GITHUB_WORKSPACE: $buildRoot"
}

if ($env:MATRIX_CI_COLD_BUILD -eq '1' -and (Test-Path $buildRoot)) {
    Remove-Item -LiteralPath $buildRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $buildRoot | Out-Null
$xmakeBuildRoot = Join-Path $workspace ("build-windows-$variant")
Ensure-VerifiedJunction -LinkPath $xmakeBuildRoot -TargetPath $buildRoot
if (Test-Path (Join-Path $env:GITHUB_WORKSPACE '.xmake')) {
    Remove-Item -LiteralPath (Join-Path $env:GITHUB_WORKSPACE '.xmake') -Recurse -Force
}
foreach ($sourceSpec in @(
        @{ Name = 'commonlibf4-ng'; Root = (Join-Path $workspace 'framework/prismaui-core/lib/commonlibf4vr/CommonLibF4/src') },
        @{ Name = 'commonlibf4'; Root = (Join-Path $workspace 'shared/CommonLibF4/lib/commonlib-shared/src') })) {
    if (Test-Path -LiteralPath $sourceSpec.Root) {
        Get-ChildItem -LiteralPath $sourceSpec.Root -File -Recurse -Filter '*.cpp' | ForEach-Object {
            $relativeDirectory = $_.DirectoryName.Substring($workspace.Length).TrimStart('\')
            New-Item -ItemType Directory -Force -Path (Join-Path $buildRoot (Join-Path ".deps\$($sourceSpec.Name)\windows\x64\release" $relativeDirectory)) | Out-Null
        }
    }
}

$configArgs = @(
    'config', '--file=xmake-xwin-framework.lua', '--plat=windows', '--arch=x64',
    '--mode=release', "--builddir=$xmakeBuildRoot", '--yes'
)
if ($Target -eq 'PrismaUI_F4VR') {
    $configArgs += @('--fallout_f4=n', '--fallout_f4ng=n', '--fallout_f4vr=y', '--cxflags=-fdelayed-template-parsing')
}
if ($AcceleratedAcceptance) {
    $configArgs += '--prisma_accelerated_runtime_acceptance=y'
}
if ($Diagnostics) {
    $configArgs += '--cxflags=-DPRISMA_DIAGNOSTICS'
}

Write-Host "Persistent build cache: $buildRoot"
Write-Host "Cache fingerprint: $fingerprint"

& xmake @configArgs
if ($LASTEXITCODE -ne 0) { throw "xmake configure failed for $Target" }
$stalePchFiles = Get-ChildItem -LiteralPath $buildRoot -Recurse -Force -ErrorAction SilentlyContinue |
    Where-Object { -not $_.PSIsContainer -and ($_.Name -like '*.pch' -or $_.Name -like '*.pch.obj') }
foreach ($stalePchFile in $stalePchFiles) {
    Remove-Item -LiteralPath $stalePchFile.FullName -Force
}
& xmake build --file=xmake-xwin-framework.lua --jobs=1 -vD $Target
if ($LASTEXITCODE -ne 0) { throw "xmake build failed for $Target" }

# The ModelPreview Interface3D prototype is dead code behind PRISMA_MODELPREVIEW_INTERFACE3D_PROTOTYPE,
# which no provider target defines: the flat target globs its .cpp, so it compiles as an EMPTY
# translation unit everywhere and a real error in its body never reaches a build. Two had accumulated
# -- an invalid enum qualifier and a windows.h-before-CommonLib include order. Compile the body
# explicitly, one extra translation unit in a job that already compiles this file, so the next one
# fails here instead of sitting in the tree. Flat only: ModelPreview is flat-only, and the
# accelerated/diagnostics/localization variants do not need to repeat it.
if ($Target -eq 'PrismaUI_F4' -and -not $AcceleratedAcceptance -and -not $Diagnostics -and
        -not $LocalizationAcceptance) {
    & xmake build --file=xmake-xwin-framework.lua --jobs=1 -vD PrismaModelPreviewPrototype
    if ($LASTEXITCODE -ne 0) { throw 'xmake build failed for PrismaModelPreviewPrototype' }
}

$acceptanceDll = $null
if ($LocalizationAcceptance) {
    & xmake build --file=xmake-xwin-framework.lua --jobs=1 -vD PrismaLocalizationAcceptance
    if ($LASTEXITCODE -ne 0) { throw 'xmake build failed for PrismaLocalizationAcceptance' }
    $acceptanceDll = Get-ChildItem -LiteralPath $buildRoot -Recurse -File -Filter 'PrismaLocalizationAcceptance.dll' |
        Select-Object -First 1
    if (-not $acceptanceDll) { throw 'PrismaLocalizationAcceptance.dll was not produced under $buildRoot' }
    $acceptanceValidatorArgs = @(
        (Join-Path $workspace '.github\scripts\validate-pe-x64.py'),
        $acceptanceDll.FullName,
        '--require-export', 'F4SEPlugin_Query',
        '--require-export', 'F4SEPlugin_Load',
        '--require-export', 'F4SEPlugin_Version',
        '--forbid-export', 'RequestPluginAPI',
        '--forbid-export', 'PrismaUI_F4_GetAPI',
        '--forbid-export', 'RequestPluginVRAPI'
    )
    & python @acceptanceValidatorArgs
    if ($LASTEXITCODE -ne 0) { throw "PE/export validation failed for $($acceptanceDll.Name)" }
}

$controllerTestDll = $null
if ($ControllerTest) {
    & xmake build --file=xmake-xwin-framework.lua --jobs=1 -vD PrismaControllerTest_F4
    if ($LASTEXITCODE -ne 0) { throw 'xmake build failed for PrismaControllerTest_F4' }
    $controllerTestDll = Get-ChildItem -LiteralPath $buildRoot -Recurse -File -Filter 'PrismaControllerTest_F4.dll' |
        Select-Object -First 1
    if (-not $controllerTestDll) { throw 'PrismaControllerTest_F4.dll was not produced under $buildRoot' }
    & python (Join-Path $workspace '.github\scripts\validate-pe-x64.py') $controllerTestDll.FullName `
        --require-export F4SEPlugin_Query --require-export F4SEPlugin_Load --require-export F4SEPlugin_Version `
        --forbid-export RequestPluginAPI --forbid-export PrismaUI_F4_GetAPI
    if ($LASTEXITCODE -ne 0) { throw "PE/export validation failed for $($controllerTestDll.Name)" }
}

$dll = Get-ChildItem -LiteralPath $buildRoot -Recurse -File -Filter ("$Target.dll") | Select-Object -First 1
$pdb = Get-ChildItem -LiteralPath $buildRoot -Recurse -File -Filter ("$Target.pdb") | Select-Object -First 1
if (-not $dll) { throw "$Target.dll was not produced under $buildRoot" }
if (-not $pdb) { throw "$Target.pdb was not produced under $buildRoot" }

$validatorArgs = @(
    (Join-Path $workspace '.github\scripts\validate-pe-x64.py'),
    $dll.FullName,
    '--require-export', 'F4SEPlugin_Query',
    '--require-export', 'F4SEPlugin_Load',
    '--require-export', 'F4SEPlugin_Version',
    '--require-export', 'RequestPluginAPI',
    '--require-export', 'PrismaUI_F4_GetAPI',
    '--require-export', 'PrismaUI_F4_GetCapabilities',
    '--require-export', 'PrismaUI_F4_SetViewCursorPolicy',
    '--require-export', 'PrismaUI_F4_GetViewCursorPolicy',
    '--require-export', 'PrismaUI_F4_RegisterTranslationsV4'
)
if ($Target -eq 'PrismaUI_F4VR') {
    $validatorArgs += @('--require-export', 'RequestPluginVRAPI')
} else {
    $validatorArgs += @('--forbid-export', 'RequestPluginVRAPI', '--require-export', 'PrismaUI_F4_GetRuntimeState')
}
& python @validatorArgs
if ($LASTEXITCODE -ne 0) { throw "PE/export validation failed for $($dll.Name)" }

if (-not [string]::IsNullOrWhiteSpace($ExportRoot)) {
    $exportBase = if ([System.IO.Path]::IsPathRooted($ExportRoot)) {
        [System.IO.Path]::GetFullPath($ExportRoot)
    } else {
        [System.IO.Path]::GetFullPath((Join-Path $workspace $ExportRoot))
    }
    $releaseRoot = Join-Path $exportBase 'windows\x64\release'
    New-Item -ItemType Directory -Force -Path $releaseRoot | Out-Null
    Copy-Item -LiteralPath $dll.FullName -Destination (Join-Path $releaseRoot $dll.Name) -Force
    Copy-Item -LiteralPath $pdb.FullName -Destination (Join-Path $releaseRoot $pdb.Name) -Force
    if ($acceptanceDll) {
        Copy-Item -LiteralPath $acceptanceDll.FullName -Destination (Join-Path $releaseRoot $acceptanceDll.Name) -Force
    }
    if ($controllerTestDll) {
        Copy-Item -LiteralPath $controllerTestDll.FullName -Destination (Join-Path $releaseRoot $controllerTestDll.Name) -Force
    }
    Write-Host "exported release artifacts to $releaseRoot"
}
Write-Host "validated Windows PE artifact: $($dll.FullName)"
