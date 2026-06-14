@echo off
setlocal
cd /d "%~dp0"

set "BIN=%~dp0build\Release"

if not exist "%BIN%\fireice_client.exe" (
    echo Please run build.bat first.
    exit /b 1
)

echo === Fire & Ice 联机测试 ===
echo.
echo [左侧窗口] 主机 - 点击"创建房间"
echo [右侧窗口] 加入方 - 点击"加入房间"，会自动扫描到主机
echo.

start "FireIce-Host" /D "%BIN%" "%BIN%\fireice_client.exe"
ping -n 4 127.0.0.1 >nul
start "FireIce-Guest" /D "%BIN%" "%BIN%\fireice_client.exe"

echo.
echo 使用说明:
echo   主机:  点击 [创建房间] → 房间号和IP显示在左上角
echo   加入方: 点击 [加入房间] → 自动扫描局域网 → 选择房间 → Enter 加入
echo.
echo 两台电脑联机（同一WiFi/局域网）:
echo   两台电脑都运行 fireice_client.exe
echo   一台创建房间，另一台加入房间即可自动发现
echo.
