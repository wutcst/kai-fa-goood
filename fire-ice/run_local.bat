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

start "" /D "%BIN%" "%BIN%\fireice_client.exe"
