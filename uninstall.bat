@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

echo ========================================
echo Virtual CDROM Driver Uninstallation
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
set "CONTROL_EXE=build\x64\VcdControl.exe"
set "SYSTEM_DRIVERS=C:\Windows\System32\drivers"

:: Check if control utility exists
if not exist "%CONTROL_EXE%" (
    echo Error: Control utility not found: %CONTROL_EXE%
    pause
    exit /b 1
)

echo Uninstallation steps:
echo   1. Stopping driver service...
echo   2. Uninstalling driver service...
echo   3. Removing driver files...
echo.

:: Stop driver
echo Stopping driver...
"%CONTROL_EXE%" stop
if errorlevel 1 (
    echo Warning: Failed to stop driver cleanly, continuing...
)

timeout /t 2 /nobreak >nul

:: Uninstall driver
echo Uninstalling driver service...
"%CONTROL_EXE%" uninstall
if errorlevel 1 (
    echo Warning: Driver uninstallation may have issues, continuing...
)

:: Remove driver file from system directory
if exist "%SYSTEM_DRIVERS%\%DRIVER_NAME%.sys" (
    echo Removing driver file from system directory...
    del /F "%SYSTEM_DRIVERS%\%DRIVER_NAME%.sys" >nul 2>&1
    if errorlevel 1 (
        echo Warning: Could not remove driver file. It may be in use.
    ) else (
        echo Driver file removed.
    )
)

echo.
echo ========================================
echo Uninstallation completed!
echo ========================================
echo.

endlocal
pause
