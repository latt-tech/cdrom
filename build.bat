@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

echo ========================================
echo Virtual CDROM Driver Build Script
echo ========================================
echo.

:: Check for Visual Studio
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo Error: Visual Studio not found.
    echo Please install Visual Studio with Windows Driver Kit (WDK).
    exit /b 1
)

:: Find Visual Studio installation
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VSINSTALLPATH=%%i"
)

if not defined VSINSTALLPATH (
    echo Error: Visual Studio with C++ tools not found.
    exit /b 1
)

echo Found Visual Studio at: %VSINSTALLPATH%

:: Setup environment
call "%VSINSTALLPATH%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
if errorlevel 1 (
    echo Error: Failed to setup Visual Studio environment.
    exit /b 1
)

:: Check for WDK
if not defined WDKContentRoot (
    echo Error: Windows Driver Kit (WDK) not found.
    echo Please install WDK from: https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk
    exit /b 1
)

echo Found WDK at: %WDKContentRoot%
echo.

:: Create output directories
if not exist "build" mkdir "build"
if not exist "build\x64" mkdir "build\x64"

:: Build driver
echo Building Virtual CDROM Driver...
echo.

msbuild VirtualCdrom.sln /p:Configuration=Release /p:Platform=x64 /p:OutDir=..\build\x64\
if errorlevel 1 (
    echo Error: Driver build failed.
    exit /b 1
)

echo.
echo Building Control Utility...
echo.

msbuild VcdControl.vcxproj /p:Configuration=Release /p:Platform=x64 /p:OutDir=..\build\x64\
if errorlevel 1 (
    echo Error: Control utility build failed.
    exit /b 1
)

echo.
echo ========================================
echo Build completed successfully!
echo ========================================
echo.
echo Output files:
echo   - build\x64\VirtualCdrom.sys  (Driver)
echo   - build\x64\VcdControl.exe   (Control utility)
echo   - VirtualCdrom.inf           (Installation file)
echo.
echo To install and use:
echo   1. Copy build\x64\VirtualCdrom.sys to C:\Windows\System32\drivers\
echo   2. Run: VcdControl.exe install
echo   3. Run: VcdControl.exe start
echo.
echo Or use the install.bat script.
echo.

endlocal
