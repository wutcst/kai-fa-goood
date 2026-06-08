@echo off
setlocal
cd /d "%~dp0"

set "BIN=%~dp0build\Release"

if not exist "%BIN%\fireice_server.exe" (
    echo Please run build.bat first.
    exit /b 1
)

start "FireIce Server" /D "%BIN%" "%BIN%\fireice_server.exe"
ping -n 2 127.0.0.1 >nul
start "Fire Boy" /D "%BIN%" "%BIN%\fireice_client.exe" 127.0.0.1 fire
ping -n 2 127.0.0.1 >nul
start "Water Girl" /D "%BIN%" "%BIN%\fireice_client.exe" 127.0.0.1 water

echo Started server and two clients from %BIN%
