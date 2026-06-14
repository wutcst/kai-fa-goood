@echo off
setlocal

cd /d "%~dp0"

set "BIN=%~dp0build\Release"

if not exist "%BIN%\fireice_server.exe" (
    echo Please run build.bat first.
    exit /b 1
)

if not exist "%BIN%\fireice_client.exe" (
    echo fireice_client.exe not found. Please run build.bat first.
    exit /b 1
)

if not exist "%BIN%\levels\level01_collision.txt" (
    echo Level files missing in %BIN%\levels
    echo Please run build.bat to copy assets.
    exit /b 1
)

echo Closing previous game instances...
taskkill /F /IM fireice_client.exe >nul 2>&1
taskkill /F /IM fireice_server.exe >nul 2>&1
ping -n 2 127.0.0.1 >nul

start "FireIce Server" /D "%BIN%" "%BIN%\fireice_server.exe"
ping -n 2 127.0.0.1 >nul
start "Fire-Ice Client" /D "%BIN%" "%BIN%\fireice_client.exe" 127.0.0.1 fire

echo Started server and one client from %BIN%
echo.
echo In the client window: select create room and press Enter to connect.
echo To play co-op, open another client manually:
echo   %BIN%\fireice_client.exe 127.0.0.1 water
