@echo off
cd /d "%~dp0"
set "BIN=%~dp0build\Release"

if not exist "%BIN%\fireice_client.exe" (
    echo Please run build.bat first.
    pause
    exit /b 1
)

call "%~dp0sync_assets.bat"
if errorlevel 1 (
    pause
    exit /b 1
)

echo Starting local server...
start "Fire-Ice Server" /D "%BIN%" "%BIN%\fireice_server.exe"
ping 127.0.0.1 -n 2 >nul

echo Starting client (local server 127.0.0.1)...
start "Fire-Ice" /D "%BIN%" "%BIN%\fireice_client.exe" 127.0.0.1
