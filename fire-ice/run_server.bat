@echo off
setlocal
cd /d "%~dp0"

set "BIN=%~dp0build\Release"
set "SERVER=8.141.101.126"

if not exist "%BIN%\fireice_client.exe" (
    echo Please run build.bat first.
    exit /b 1
)

echo === Fire & Ice - 连接远程服务器 %SERVER% ===
echo.

start "FireIce-Fire" /D "%BIN%" "%BIN%\fireice_client.exe" %SERVER% fire
ping -n 3 127.0.0.1 >nul
start "FireIce-Water" /D "%BIN%" "%BIN%\fireice_client.exe" %SERVER% water

echo.
echo 使用说明:
echo   两个窗口都点击 [加入房间]
echo   直接按 Enter（不输入房间号）= 快速加入
echo   或者输入房间号后按 Enter
echo.
echo 远程服务器: %SERVER%:24567
echo.
