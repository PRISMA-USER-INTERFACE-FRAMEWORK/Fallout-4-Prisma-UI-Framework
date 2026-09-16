@echo off
setlocal enabledelayedexpansion

echo ========================================
echo   PrismaUI_F4 2.1.0 Example Plugin
echo   Build and Deploy
echo ========================================
echo.

echo Framework requirement:
echo   PrismaUI_F4 2.1.0 must already be installed separately.
echo   Release: https://github.com/PRISMA-USER-INTERFACE-FRAMEWORK/Prisma-Matrix/releases/tag/framework-v2.1.0
echo   Supported desktop runtime families: Fallout 4 1.10.163 OG and 1.11.137+ AE.
echo   Fallout 4 1.10.980-1.10.984 is not supported by PrismaUI_F4 2.1.0.
echo.

REM --- Set up Visual Studio environment if not already done ---
if not defined VCINSTALLDIR (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if not exist "!VSWHERE!" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
    if not exist "!VSWHERE!" (
        echo ERROR: Cannot find vswhere.exe. Install Visual Studio 2022 with Desktop development with C++.
        pause
        exit /b 1
    )

    for /f "tokens=*" %%I in ('"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath') do set "VS_PATH=%%I"
    if not defined VS_PATH (
        echo ERROR: No Visual Studio installation with the required C++ tools was found.
        pause
        exit /b 1
    )

    call "!VS_PATH!\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    if errorlevel 1 (
        echo ERROR: Failed to initialize the Visual Studio C++ environment.
        pause
        exit /b 1
    )
    echo Visual Studio environment initialized.
)

where xmake >nul 2>&1
if errorlevel 1 (
    echo ERROR: xmake was not found in PATH.
    echo Install xmake 3.0 or later, then run this script again.
    pause
    exit /b 1
)

echo.
echo ========================================
echo STEP 1: Build PrismaUI-F4-Example
echo ========================================
echo.
cd /d "%~dp0"
xmake f -c -P . -m release
if errorlevel 1 (
    echo ERROR: xmake configuration failed.
    pause
    exit /b 1
)

xmake -P .
if errorlevel 1 (
    echo ERROR: Build failed.
    pause
    exit /b 1
)

set "PLUGIN_DLL=%~dp0build\windows\x64\release\PrismaUI-F4-Example.dll"
if not exist "!PLUGIN_DLL!" (
    echo ERROR: Expected build output was not found:
    echo   !PLUGIN_DLL!
    pause
    exit /b 1
)

echo Build successful.
echo.
echo ========================================
echo STEP 2: Deploy consumer plugin files
echo ========================================
echo.
echo Enter the target Fallout 4 Data directory or the Data root inside your MO2 mod.
set /p DEPLOY_PATH="Data path: "

if "!DEPLOY_PATH!"=="" (
    echo ERROR: No deployment path provided.
    pause
    exit /b 1
)

if not exist "!DEPLOY_PATH!" (
    echo ERROR: Deployment path does not exist: !DEPLOY_PATH!
    pause
    exit /b 1
)

if not exist "!DEPLOY_PATH!\F4SE\Plugins" mkdir "!DEPLOY_PATH!\F4SE\Plugins"
if errorlevel 1 (
    echo ERROR: Failed to create F4SE\Plugins.
    pause
    exit /b 1
)

if not exist "!DEPLOY_PATH!\PrismaUI_F4\views\PrismaUI-F4-Example" mkdir "!DEPLOY_PATH!\PrismaUI_F4\views\PrismaUI-F4-Example"
if errorlevel 1 (
    echo ERROR: Failed to create the PrismaUI view directory.
    pause
    exit /b 1
)

echo Deploying plugin DLL...
copy /Y "!PLUGIN_DLL!" "!DEPLOY_PATH!\F4SE\Plugins\PrismaUI-F4-Example.dll" >nul
if errorlevel 1 (
    echo ERROR: Failed to copy the plugin DLL.
    pause
    exit /b 1
)

if not exist "%~dp0view\index.html" (
    echo ERROR: Example view files were not found under %~dp0view\
    pause
    exit /b 1
)

echo Deploying view files...
xcopy /Y /E /I "%~dp0view\*" "!DEPLOY_PATH!\PrismaUI_F4\views\PrismaUI-F4-Example\" >nul
if errorlevel 1 (
    echo ERROR: Failed to copy the example view files.
    pause
    exit /b 1
)

echo.
echo ========================================
echo   DEPLOYMENT COMPLETE
echo ========================================
echo Plugin:
echo   !DEPLOY_PATH!\F4SE\Plugins\PrismaUI-F4-Example.dll
echo View:
echo   !DEPLOY_PATH!\PrismaUI_F4\views\PrismaUI-F4-Example\
echo.
echo This script does not install or replace the PrismaUI_F4 framework runtime.
echo Install PrismaUI_F4 2.1.0 separately from the official framework release.
echo.
pause
