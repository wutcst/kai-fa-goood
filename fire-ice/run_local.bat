@echo off
cd /d "%~dp0"
set "BIN=%~dp0build\Release"

if not exist "%BIN%\fireice_client.exe" (
    echo Please run build.bat first.
    pause
    exit /b 1
)

start "Fire & Ice" /D "%BIN%" "%BIN%\fireice_client.exe" 8.141.101.126
