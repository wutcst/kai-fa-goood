@echo off
REM 本地仅启动客户端，联机服务端部署在远端（见 DEFAULT_SERVER_HOST / server\run.bat）

set "ROOT=%~dp0.."
cd /d "%ROOT%"

set "BIN=%ROOT%\build\Release"

if not exist "%BIN%\fireice_client.exe" (
    echo Please run build.bat first.
    pause
    exit /b 1
)

echo Stopping previous client...
taskkill /F /IM fireice_client.exe >nul 2>&1

call "%~dp0sync_assets.bat"
if errorlevel 1 (
    pause
    exit /b 1
)

echo Starting client (connects to remote server 8.141.101.126:24567)...
start "Fire-Ice" /D "%BIN%" "%BIN%\fireice_client.exe"
