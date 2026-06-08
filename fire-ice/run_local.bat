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

start "Fire-Ice Client" /D "%BIN%" "%BIN%\fireice_client.exe" 127.0.0.1 fire



echo Started server and one client from %BIN%

echo.

echo In the client window: select "开始游戏" and press Enter to connect.

echo To play co-op, open another client manually:

echo   %BIN%\fireice_client.exe 127.0.0.1 water

