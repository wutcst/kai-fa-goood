@echo off

cd /d "%~dp0"

set "BIN=%~dp0build\Release"



if not exist "%BIN%\fireice_client.exe" (

    echo Please run build.bat first.

    pause

    exit /b 1

)



echo Stopping previous game processes...

taskkill /F /IM fireice_client.exe >nul 2>&1

taskkill /F /IM fireice_server.exe >nul 2>&1



call "%~dp0sync_assets.bat"

if errorlevel 1 (

    pause

    exit /b 1

)



echo Starting client...

start "Fire-Ice" /D "%BIN%" "%BIN%\fireice_client.exe"

