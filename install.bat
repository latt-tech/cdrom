@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

echo ========================================
echo Virtual CDROM Driver Installation
echo ========================================
echo.

:: Check for admin privileges
net session >nul 2>&1
if errorlevel 1 (
    echo Error: Administrator privileges required.
    echo Please run this script as Administrator.
    pause
    exit /b 1
)

:: Set paths
set "DRIVER_NAME=VirtualCdrom"
set "DRIVER_SYS=build\x64\VirtualCdrom.sys"
set "DRIVER_INF=VirtualCdrom.inf"
set "CONTROL_EXE=build\x64\VcdControl.exe"
set "SYSTEM_DRIVERS=C:\Windows\System32\drivers"

:: Check if driver file exists
if not exist "%DRIVER_SYS%" (
    echo Error: Driver file not found: %DRIVER_SYS%
    echo Please run build.bat first to compile the driver.
    pause
    exit /b 1
)

if not exist "%CONTROL_EXE%" (
    echo Error: Control utility not found: %CONTROL_EXE%
    echo Please run build.bat first to compile the control utility.
    pause
    exit /b 1
)

:: Create ISO directory if it doesn't exist
if not exist "C:\Program Files (x86)\CDROM" (
    echo Creating ISO directory: C:\Program Files (x86)\CDROM
    mkdir "C:\Program Files (x86)\CDROM"
)

echo.
echo Installation steps:
echo   1. Stopping existing driver service...
echo   2. Copying driver files...
echo   3. Installing driver service...
echo   4. Starting driver...
echo.

:: Stop existing service if running
sc query %DRIVER_NAME% >nul 2>&1
if !errorlevel! equ 0 (
    echo Stopping existing driver...
    sc stop %DRIVER_NAME% >nul 2>&1
    timeout /t 2 /nobreak >nul
)

:: Copy driver to system directory
echo Copying driver to system directory...
copy /Y "%DRIVER_SYS%" "%SYSTEM_DRIVERS%\%DRIVER_NAME%.sys" >nul
if errorlevel 1 (
    echo Error: Failed to copy driver file.
    pause
    exit /b 1
)

echo Driver copied successfully.
echo.

:: Install driver using control utility
echo Installing driver service...
"%CONTROL_EXE%" install
if errorlevel 1 (
    echo Warning: Driver installation may have issues, continuing...
)

echo.
echo Starting driver...
"%CONTROL_EXE%" start
if errorlevel 1 (
    echo Error: Failed to start driver.
    pause
    exit /b 1
)

echo.
echo ========================================
echo Installation completed!
echo ========================================
echo.
echo Driver status:
"%CONTROL_EXE%" status
echo.
echo Usage:
echo   VcdControl.exe status    - Check driver status
echo   VcdControl.exe insert    - Insert media (if ISO exists)
echo   VcdControl.exe eject     - Eject media
echo   VcdControl.exe stop      - Stop driver
echo.
echo ISO file location:
echo   C:\Program Files (x86)\CDROM\vCD.iso
echo.
echo The driver will automatically detect the ISO file every 10 seconds
echo when no media is inserted and insert it automatically.
echo.
echo If media is ejected, it will not be re-inserted until device restart.
echo.
pause

endlocal
