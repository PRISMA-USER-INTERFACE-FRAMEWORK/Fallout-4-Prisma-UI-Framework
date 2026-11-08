@echo off
setlocal enabledelayedexpansion

echo ========================================
echo   PrismaUI Example Plugin
echo   Complete Build and Deploy
echo ========================================
echo.

REM --- Set up Visual Studio environment if not already done ---
if not defined VCINSTALLDIR (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if not exist "!VSWHERE!" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
    if not exist "!VSWHERE!" (
        echo ERROR: Cannot find vswhere.exe - please install Visual Studio 2022 with C++ workload.
        pause
        exit /b 1
    )
    for /f "tokens=*" %%I in ('"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath') do set "VS_PATH=%%I"
    if not defined VS_PATH (
        echo ERROR: No Visual Studio installation with C++ tools found.
        pause
        exit /b 1
    )
    call "!VS_PATH!\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    echo Visual Studio environment initialized.
)

echo.
if exist "%~dp0build\windows\x64\release\PrismaUI-F4-Example.dll" (
    if exist "%~dp0build\windows\x64\release\PrismaUI-F4-Example.dll" (
        echo Checking for existing build...

        for %%F in ("%~dp0build\windows\x64\release\PrismaUI-F4-Example.dll") do set PLUGIN_TIME=%%~tF
        set FRAMEWORK_TIME=!PLUGIN_TIME!

        if "!FRAMEWORK_TIME!" gtr "!PLUGIN_TIME!" (
            echo.
            echo ========================================
            echo   WARNING: PrismaUI_F4 FRAMEWORK UPDATED
            echo ========================================
            echo The PrismaUI_F4 framework DLL is NEWER than this plugin.
            echo You MUST rebuild this plugin against the new framework.
            echo.
            set /p CONTINUE="Continue with build? (Y/N): "
            if /i not "!CONTINUE!"=="Y" (
                echo Build cancelled.
                pause
                exit /b 0
            )
        )
    )
)

echo ========================================
echo STEP 1: Building PrismaUI-F4-Example...
echo ========================================
echo.
echo Building (single DLL - supports OG 1.10.163 + AE 1.11.137 and later)...
cd /d "%~dp0"
xmake f -c -P .
xmake -P .
if errorlevel 1 (
    echo.
    echo ERROR: Build failed
    pause
    exit /b 1
)

echo Build successful!
echo.

if not exist "build\windows\x64\release\PrismaUI-F4-Example.dll" (
    echo.
    echo ========================================
    echo   ERROR: Build Output Not Found
    echo ========================================
    echo The plugin DLL was not created.
    echo Check the xmake output above for errors.
    echo.
    pause
    exit /b 1
)

echo ========================================
echo STEP 2: Deployment
echo ========================================
echo.
set /p DEPLOY_PATH="Enter deployment path: "

if "!DEPLOY_PATH!"=="" (
    echo ERROR: No path provided
    pause
    exit /b 1
)

if not exist "!DEPLOY_PATH!" (
    echo ERROR: Deployment path does not exist: !DEPLOY_PATH!
    pause
    exit /b 1
)

echo.
echo Creating directories...
if not exist "!DEPLOY_PATH!\F4SE\plugins" mkdir "!DEPLOY_PATH!\F4SE\plugins"
if not exist "!DEPLOY_PATH!\PrismaUI_F4\views\PrismaUI-F4-Example" mkdir "!DEPLOY_PATH!\PrismaUI_F4\views\PrismaUI-F4-Example"

echo Deploying DLL...
copy /Y "build\windows\x64\release\PrismaUI-F4-Example.dll" "!DEPLOY_PATH!\F4SE\plugins\PrismaUI-F4-Example.dll"
if errorlevel 1 (
    echo ERROR: Failed to copy DLL
    pause
    exit /b 1
)

echo Deploying view files...
if not exist "view\index.html" (
    echo ERROR: View files not found at view\
    pause
    exit /b 1
)
xcopy /Y /I "view\*" "!DEPLOY_PATH!\PrismaUI_F4\views\PrismaUI-F4-Example\" >nul

echo Deploying license...
if exist "..\LICENSE.md" copy /Y "..\LICENSE.md" "!DEPLOY_PATH!\LICENSE.md" >nul

echo.
echo ========================================
echo   SUCCESS: Deployment Complete!
echo ========================================
echo Plugin deployed to: !DEPLOY_PATH!
echo.
pause
