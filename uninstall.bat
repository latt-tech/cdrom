@echo off

rem 卸载虚拟DVD驱动
set DRIVER_NAME=VirtualCdrom

rem 检查驱动是否已安装
sc query %DRIVER_NAME% >nul 2>&1
if %errorlevel% equ 0 (
    echo 正在停止驱动...
    sc stop %DRIVER_NAME%
    echo 正在删除驱动...
    sc delete %DRIVER_NAME%
    echo 驱动卸载成功！
) else (
    echo 驱动未安装！
)

pause