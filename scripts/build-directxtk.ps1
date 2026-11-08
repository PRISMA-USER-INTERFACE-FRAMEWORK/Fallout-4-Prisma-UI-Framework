param([switch]$Fresh)

$ErrorActionPreference = 'Stop'
$llvmBin = if ($env:PRISMA_LLVM_BIN) { $env:PRISMA_LLVM_BIN } else { 'C:\Program Files\LLVM\bin' }
$xwinRoot = if ($env:XWIN_ROOT) { $env:XWIN_ROOT } else { 'C:\PrismaToolchain\xwin' }
$clang = (Resolve-Path (Join-Path $llvmBin 'clang-cl.exe')).Path
$linker = (Resolve-Path (Join-Path $llvmBin 'lld-link.exe')).Path
$libtool = (Resolve-Path (Join-Path $llvmBin 'llvm-lib.exe')).Path
$sourceHash = 'edb643b2444ff24925339cfb1bc9f76c671d5404a5549d32ecaa0d61bbab28c9'
$fingerprint = @($clang, (@(& $clang --version)[0]), $linker,
    (@(& $linker --version)[0]), $xwinRoot, $sourceHash) -join "`n"
$digest = [Security.Cryptography.SHA256]::Create().ComputeHash([Text.Encoding]::UTF8.GetBytes($fingerprint))
$key = (($digest | ForEach-Object { $_.ToString('x2') }) -join '').Substring(0, 20)
$cacheBase = Join-Path ([IO.Path]::GetTempPath()) 'prisma-dxtk'
$suffix = if ($Fresh -or $env:MATRIX_CI_COLD_DIRECTXTK -eq '1') { "-$($env:GITHUB_RUN_ID)-$($env:GITHUB_RUN_ATTEMPT)-$($env:GITHUB_JOB)-fresh" } else { '' }
$root = if ($env:PRISMA_DIRECTXTK_ROOT) { Split-Path $env:PRISMA_DIRECTXTK_ROOT -Parent } else { Join-Path $cacheBase "directxtk-24.2.0-$key$suffix" }
$install = Join-Path $root 'install'
$lib = Join-Path $install 'lib\DirectXTK.lib'

if (-not (Test-Path -LiteralPath $lib -PathType Leaf)) {
    New-Item -ItemType Directory -Force -Path $root | Out-Null
    $archive = Join-Path $root 'feb2024.zip'
    foreach ($attempt in 1..3) {
        try {
            Invoke-WebRequest 'https://github.com/microsoft/DirectXTK/archive/feb2024.zip' -OutFile $archive
            break
        } catch {
            if ($attempt -eq 3) { throw }
            Write-Warning "DirectXTK download attempt $attempt failed: $($_.Exception.Message); retrying"
            Start-Sleep -Seconds (10 * $attempt)
        }
    }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sourceHash) {
        throw 'DirectXTK 24.2.0 source checksum mismatch'
    }
    Expand-Archive -LiteralPath $archive -DestinationPath $root -Force
    $source = Join-Path $root 'DirectXTK-feb2024'
    $env:INCLUDE = (@('crt\include', 'sdk\include\ucrt', 'sdk\include\um', 'sdk\include\shared', 'sdk\include\winrt') |
        ForEach-Object { Join-Path $xwinRoot $_ }) -join ';'
    $env:LIB = (@('crt\lib\x86_64', 'sdk\lib\ucrt\x86_64', 'sdk\lib\um\x86_64') |
        ForEach-Object { Join-Path $xwinRoot $_ }) -join ';'
    $libFlags = (($env:LIB -split ';') | ForEach-Object { "/libpath:`"$_`"" }) -join ' '
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vsInstall = & $vswhere -latest -products '*' -property installationPath
    if (-not $vsInstall) { throw 'Visual Studio installation was not found' }
    $cmake = if (Get-Command cmake.exe -ErrorAction SilentlyContinue) { (Get-Command cmake.exe).Source } else { Join-Path $vsInstall 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' }
    $ninja = if (Get-Command ninja.exe -ErrorAction SilentlyContinue) { (Get-Command ninja.exe).Source } else { Join-Path $vsInstall 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe' }
    $sdkBin = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin\$env:PRISMA_WINDOWS_SDK_VERSION\x64"
    foreach ($tool in @($cmake, $ninja, (Join-Path $sdkBin 'fxc.exe'))) {
        if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) { throw "DirectXTK build tool missing: $tool" }
    }
    $env:PATH = "$llvmBin;$sdkBin;$env:PATH"
    & $cmake -S $source -B (Join-Path $root 'build') -G Ninja `
        "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_CXX_COMPILER=$clang" `
        "-DCMAKE_LINKER=$linker" "-DCMAKE_AR=$libtool" `
        '-DCMAKE_CXX_COMPILER_TARGET=x86_64-pc-windows-msvc' `
        '-DCMAKE_CXX_FLAGS=/Z7 -fuse-ld=lld' "-DCMAKE_EXE_LINKER_FLAGS=$libFlags" `
        '-DCMAKE_BUILD_TYPE=Release' '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL' `
        "-DCMAKE_INSTALL_PREFIX=$install" '-DCMAKE_INSTALL_LIBDIR=lib' `
        '-DBUILD_TOOLS=OFF' '-DBUILD_TESTING=OFF'
    if ($LASTEXITCODE -ne 0) { throw 'DirectXTK configure failed' }
    & $cmake --build (Join-Path $root 'build') --config Release --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'DirectXTK build failed' }
    & $cmake --install (Join-Path $root 'build') --config Release
    if ($LASTEXITCODE -ne 0) { throw 'DirectXTK install failed' }
    New-Item -ItemType Directory -Force -Path (Join-Path $install 'DirectXTK') | Out-Null
    Copy-Item -Path (Join-Path $source 'Inc\*') -Destination (Join-Path $install 'DirectXTK') -Force
}

& (Join-Path $PSScriptRoot 'verify-directxtk.ps1') -Root $install
if (-not $?) { throw 'DirectXTK compiler provenance failed' }
$env:PRISMA_DIRECTXTK_ROOT = $install
if ($env:GITHUB_ENV) { [IO.File]::AppendAllText($env:GITHUB_ENV, "PRISMA_DIRECTXTK_ROOT=$install`n", [Text.UTF8Encoding]::new($false)) }
Write-Host "DirectXTK 24.2.0: $install"
