@echo off
setlocal

set "ROOT=%~dp0"
cd /d "%ROOT%"

set "CMAKE_EXE=cmake"
where cmake >nul 2>&1
if errorlevel 1 set "CMAKE_EXE=C:\Program Files\CMake\bin\cmake.exe"

echo Checking for running game processes...
taskkill /F /IM fireice_client.exe >nul 2>&1
taskkill /F /IM fireice_server.exe >nul 2>&1

if not exist build mkdir build
cd build

"%CMAKE_EXE%" .. -G "Visual Studio 17 2022" -A x64
if errorlevel 1 exit /b 1

"%CMAKE_EXE%" --build . --config Release
if errorlevel 1 (
    echo.
    echo Build failed. If you see LNK1104, close all Fire-Ice windows and run build.bat again.
    exit /b 1
)

echo.
echo Build complete.
echo   Client:  start.bat  ^(or client\run.bat^)
echo   Server:  server\run.bat
echo   Dev:     build\Release\fireice_client.exe 127.0.0.1 fire
