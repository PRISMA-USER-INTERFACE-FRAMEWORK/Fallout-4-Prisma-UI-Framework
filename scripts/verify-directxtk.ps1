param([Parameter(Mandatory = $true)][string]$Root)

$ErrorActionPreference = 'Stop'
$rootPath = [IO.Path]::GetFullPath($Root)
$lib = Join-Path $rootPath 'lib\DirectXTK.lib'
$cache = Join-Path (Split-Path $rootPath -Parent) 'build\CMakeCache.txt'
$llvmBin = if ($env:PRISMA_LLVM_BIN) { $env:PRISMA_LLVM_BIN } else { 'C:\Program Files\LLVM\bin' }
$clang = (Resolve-Path (Join-Path $llvmBin 'clang-cl.exe')).Path
$linker = (Resolve-Path (Join-Path $llvmBin 'lld-link.exe')).Path
$libtool = (Resolve-Path (Join-Path $llvmBin 'llvm-lib.exe')).Path
$readobj = (Resolve-Path (Join-Path $llvmBin 'llvm-readobj.exe')).Path
if (-not (Test-Path -LiteralPath $lib -PathType Leaf)) { throw "DirectXTK library missing: $lib" }
if (-not (Test-Path -LiteralPath (Join-Path $rootPath 'DirectXTK\SpriteBatch.h'))) { throw 'DirectXTK headers missing' }
if (-not (Test-Path -LiteralPath $cache -PathType Leaf)) { throw 'DirectXTK CMake provenance missing' }

$settings = Get-Content -LiteralPath $cache
foreach ($entry in @(@('CMAKE_CXX_COMPILER', $clang), @('CMAKE_LINKER', $linker), @('CMAKE_AR', $libtool))) {
    $actual = $settings | Where-Object { $_ -match "^$($entry[0]):[^=]+=" } | Select-Object -First 1
    if (-not $actual -or (($actual -split '=', 2)[1] -replace '\\', '/') -ne ($entry[1] -replace '\\', '/')) {
        throw "DirectXTK $($entry[0]) did not use pinned $($entry[1])"
    }
}
$version = (@(& $clang --version)[0]).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Unable to read pinned clang-cl version' }
$producers = @(& $readobj --codeview $lib | Select-String 'VersionName:' | ForEach-Object { $_.Line.Trim() })
if ($LASTEXITCODE -ne 0 -or $producers.Count -eq 0) { throw 'DirectXTK library has no CodeView compiler records' }
if (@($producers | Where-Object { $_ -notlike "VersionName: $version*" }).Count -gt 0) {
    throw "DirectXTK library contains objects not produced by $version"
}
Write-Host "DirectXTK compiler provenance passed: $($producers.Count) clang-cl objects; $((Get-FileHash $lib -Algorithm SHA256).Hash)"
