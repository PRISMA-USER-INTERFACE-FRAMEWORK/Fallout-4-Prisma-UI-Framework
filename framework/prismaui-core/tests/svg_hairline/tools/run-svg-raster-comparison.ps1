param(
    [Parameter(Mandatory = $true)]
    [string]$BaselineRuntime,
    [Parameter(Mandatory = $true)]
    [string]$CandidateArchive,
    [string]$OutputDir = "build/svg-raster-comparison"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Resolve-ExistingDirectory([string]$Path, [string]$Label) {
    $resolved = Resolve-Path -LiteralPath $Path -ErrorAction Stop
    if (-not (Test-Path -LiteralPath $resolved -PathType Container)) {
        throw "$Label is not a directory: $Path"
    }
    return $resolved.Path
}

function Resolve-ExistingFile([string]$Path, [string]$Label) {
    $resolved = Resolve-Path -LiteralPath $Path -ErrorAction Stop
    if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
        throw "$Label is not a file: $Path"
    }
    return $resolved.Path
}

function Find-RuntimeBin([string]$RuntimeRoot) {
    $bin = Join-Path $RuntimeRoot "bin"
    if (Test-Path -LiteralPath $bin -PathType Container) { return $bin }
    $libs = Join-Path $RuntimeRoot "libs"
    if (Test-Path -LiteralPath $libs -PathType Container) { return $libs }
    throw "runtime has neither bin nor libs directory: $RuntimeRoot"
}

function Invoke-RasterMeasurement(
    [string]$Label,
    [string]$Sdk,
    [string]$Runtime,
    [string]$BuildDir,
    [string]$OutputFile
) {
    $oldSdk = $env:ULTRALIGHT_SDK_DIR
    $oldRuntime = $env:ULTRALIGHT_RUNTIME_DIR
    $oldLabel = $env:PRISMA_ULTRALIGHT_RUNTIME_LABEL
    $oldPath = $env:PATH
    try {
        $env:ULTRALIGHT_SDK_DIR = $Sdk
        $env:ULTRALIGHT_RUNTIME_DIR = $Runtime
        $env:PRISMA_ULTRALIGHT_RUNTIME_LABEL = $Label
        $env:PATH = (Find-RuntimeBin $Runtime) + ";" + $oldPath

        xmake config --file=xmake-svg-raster.lua --plat=windows --arch=x64 --mode=release --builddir=$BuildDir --yes
        if ($LASTEXITCODE -ne 0) { throw "xmake configure failed for $Label" }
        xmake build --file=xmake-svg-raster.lua --jobs=2 PrismaUI_UltralightSvgRasterAcceptance
        if ($LASTEXITCODE -ne 0) { throw "xmake build failed for $Label" }

        $exe = Get-ChildItem -Path $BuildDir -Recurse -Filter PrismaUI_UltralightSvgRasterAcceptance.exe |
            Select-Object -First 1
        if (-not $exe) { throw "SVG raster executable was not produced for $Label" }

        & $exe.FullName "framework/prismaui-core/tests/svg_hairline" 2>&1 |
            Tee-Object -FilePath $OutputFile
        if ($LASTEXITCODE -ne 0) { throw "SVG raster measurement failed for $Label" }
    }
    finally {
        $env:ULTRALIGHT_SDK_DIR = $oldSdk
        $env:ULTRALIGHT_RUNTIME_DIR = $oldRuntime
        $env:PRISMA_ULTRALIGHT_RUNTIME_LABEL = $oldLabel
        $env:PATH = $oldPath
    }
}

$baselineSdkPath = Resolve-ExistingDirectory "framework/prismaui-core/external/ultralight-1.4.0" "baseline SDK"
$baselineRuntimePath = Resolve-ExistingDirectory $BaselineRuntime "baseline runtime"
$candidateArchivePath = Resolve-ExistingFile $CandidateArchive "candidate archive"

python framework/prismaui-core/scripts/validate-ultralight-runtime.py $baselineRuntimePath
if ($LASTEXITCODE -ne 0) { throw "baseline runtime does not match the pinned Ultralight 1.4.0 manifest" }

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$candidateSdk = Join-Path $OutputDir "ultralight-1.4.1-dev-sdk"
$candidateProvenance = Join-Path $OutputDir "ultralight-1.4.1-dev.provenance.json"
python framework/prismaui-core/tests/svg_hairline/tools/prepare-ultralight-svg-sdk.py $candidateArchivePath `
    --output $candidateSdk --provenance $candidateProvenance
if ($LASTEXITCODE -ne 0) { throw "candidate archive did not match the pinned Ultralight 1.4.1-dev intake" }
$candidateSdkPath = Resolve-ExistingDirectory $candidateSdk "candidate SDK"

$baselineLog = Join-Path $OutputDir "ultralight-1.4.0.txt"
$candidateLog = Join-Path $OutputDir "ultralight-1.4.1-dev.txt"

Invoke-RasterMeasurement "1.4.0" $baselineSdkPath $baselineRuntimePath (Join-Path $OutputDir "build-1.4.0") $baselineLog
Invoke-RasterMeasurement "1.4.1-dev" $candidateSdkPath $candidateSdkPath (Join-Path $OutputDir "build-1.4.1-dev") $candidateLog

python framework/prismaui-core/tests/svg_hairline/tools/compare-svg-raster-results.py $baselineLog $candidateLog
if ($LASTEXITCODE -ne 0) { throw "SVG raster comparison did not prove the candidate fix" }

Write-Host "SVG raster comparison passed."
Write-Host "Baseline: $baselineLog"
Write-Host "Candidate: $candidateLog"
Write-Host "Candidate provenance: $candidateProvenance"
