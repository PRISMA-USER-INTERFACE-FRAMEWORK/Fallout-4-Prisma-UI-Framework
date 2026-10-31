$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ensure-windows-junction.ps1')

$toolchainRoot = if ($env:PRISMA_WIN_TOOLCHAIN) { $env:PRISMA_WIN_TOOLCHAIN } else { 'C:\PrismaToolchain' }
$xwinRoot = Join-Path $toolchainRoot 'xwin'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsInstall = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsInstall) { throw 'Visual Studio C++ Build Tools installation was not found' }
$msvcRoot = Get-ChildItem (Join-Path $vsInstall 'VC\Tools\MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1
if (-not $msvcRoot) { throw 'MSVC tool directory was not found' }
$sdkIncludeRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Include'
$sdkVersion = Get-ChildItem $sdkIncludeRoot -Directory | Sort-Object Name -Descending | Select-Object -First 1
if (-not $sdkVersion) { throw 'Windows SDK include directory was not found' }
$sdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'

$links = @{
    (Join-Path $xwinRoot 'crt\include') = (Join-Path $msvcRoot.FullName 'include')
    (Join-Path $xwinRoot 'crt\lib\x86_64') = (Join-Path $msvcRoot.FullName 'lib\x64')
    (Join-Path $xwinRoot 'sdk\include\ucrt') = (Join-Path $sdkVersion.FullName 'ucrt')
    (Join-Path $xwinRoot 'sdk\include\um') = (Join-Path $sdkVersion.FullName 'um')
    (Join-Path $xwinRoot 'sdk\include\shared') = (Join-Path $sdkVersion.FullName 'shared')
    (Join-Path $xwinRoot 'sdk\include\winrt') = (Join-Path $sdkVersion.FullName 'winrt')
    (Join-Path $xwinRoot 'sdk\lib\ucrt\x86_64') = (Join-Path $sdkRoot "Lib\$($sdkVersion.Name)\ucrt\x64")
    (Join-Path $xwinRoot 'sdk\lib\um\x86_64') = (Join-Path $sdkRoot "Lib\$($sdkVersion.Name)\um\x64")
}

foreach ($link in $links.GetEnumerator()) {
    if (-not (Test-Path $link.Value)) { throw "required toolchain path is missing: $($link.Value)" }
    Ensure-VerifiedJunction -LinkPath $link.Key -TargetPath $link.Value
}

$packageRoot = Join-Path $env:LOCALAPPDATA '.xmake\packages'
New-Item -ItemType Directory -Force -Path $packageRoot | Out-Null

# The shipping compiler is the standalone LLVM installation, never whatever clang happens to be
# first on PATH. A Visual Studio-bundled LLVM is a different compiler and produces a binary that is
# not equivalent to the shipped one, so it is rejected instead of silently substituted.
$llvmBin = if ($env:PRISMA_LLVM_BIN) { $env:PRISMA_LLVM_BIN } else { 'C:\Program Files\LLVM\bin' }
$llvmBin = [System.IO.Path]::GetFullPath($llvmBin).TrimEnd('\')
if (-not (Test-Path -LiteralPath $llvmBin -PathType Container)) {
    throw "The pinned LLVM toolchain directory was not found: $llvmBin. Install standalone LLVM there, or set PRISMA_LLVM_BIN to its bin directory."
}
$requiredLlvmTools = @('clang-cl.exe', 'lld-link.exe', 'llvm-ar.exe', 'llvm-rc.exe', 'llvm-dlltool.exe')
$missingLlvmTools = @($requiredLlvmTools | Where-Object { -not (Test-Path -LiteralPath (Join-Path $llvmBin $_) -PathType Leaf) })
if ($missingLlvmTools.Count -gt 0) {
    throw "The pinned LLVM toolchain at $llvmBin is incomplete; missing: $($missingLlvmTools -join ', '). Install a complete standalone LLVM, or point PRISMA_LLVM_BIN at one."
}

$pathEntries = @($llvmBin, (Join-Path $msvcRoot.FullName 'bin\Hostx64\x64'), (Join-Path $env:ProgramFiles 'CMake\bin'), (Join-Path $env:ProgramFiles '7-Zip'), (Join-Path $env:ProgramFiles 'xmake'))

$env:XWIN_ROOT = $xwinRoot
$env:PRISMA_WIN_PACKAGES = $packageRoot
$env:PRISMA_WIN_TOOLCHAIN = $toolchainRoot
$env:PRISMA_MSVC_VERSION = $msvcRoot.Name
$env:PRISMA_WINDOWS_SDK_VERSION = $sdkVersion.Name
$env:PRISMA_LLVM_BIN = $llvmBin
$env:PATH = (($pathEntries -join ';') + ';' + $env:PATH)

if ($env:GITHUB_ENV) {
    "XWIN_ROOT=$xwinRoot" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
    "PRISMA_WIN_PACKAGES=$packageRoot" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
    "PRISMA_WIN_TOOLCHAIN=$toolchainRoot" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
    "PRISMA_MSVC_VERSION=$($msvcRoot.Name)" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
    "PRISMA_WINDOWS_SDK_VERSION=$($sdkVersion.Name)" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
    "PRISMA_LLVM_BIN=$llvmBin" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
}
if ($env:GITHUB_PATH) {
    $pathEntries | ForEach-Object { $_ | Out-File -FilePath $env:GITHUB_PATH -Encoding utf8 -Append }
}

Write-Host "MSVC: $($msvcRoot.Name)"
Write-Host "Windows SDK: $($sdkVersion.Name)"
Write-Host "LLVM: $llvmBin"
Write-Host "XWIN_ROOT: $xwinRoot"
Write-Host "PRISMA_WIN_PACKAGES: $packageRoot"
