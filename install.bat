@echo off

rem 安装虚拟DVD驱动
set INF_PATH=%~dp0x64\Debug\VirtualCdrom.inf
set DRIVER_NAME=VirtualCdrom

rem 检查驱动是否已安装
sc query %DRIVER_NAME% >nul 2>&1
if %errorlevel% equ 0 (
    echo 驱动已安装，正在停止...
    sc stop %DRIVER_NAME%
    sc delete %DRIVER_NAME%
)

echo 安装驱动...
devcon.exe install "%INF_PATH%" ROOT\VirtualCdrom

if %errorlevel% equ 0 (
    echo 驱动安装成功！
    echo 正在启动驱动...
    sc start %DRIVER_NAME%
    echo 驱动启动成功！
) else (
    echo 驱动安装失败！
)

pause