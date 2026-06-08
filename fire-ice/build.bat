@echo off
setlocal

set "CMAKE_EXE=cmake"
where cmake >nul 2>&1
if errorlevel 1 set "CMAKE_EXE=C:\Program Files\CMake\bin\cmake.exe"

if not exist build mkdir build
cd build

"%CMAKE_EXE%" .. -G "Visual Studio 17 2022" -A x64
if errorlevel 1 exit /b 1

"%CMAKE_EXE%" --build . --config Release
if errorlevel 1 exit /b 1

echo.
echo Build complete. Run:
echo   run_local.bat
echo   build\Release\fireice_server.exe
echo   build\Release\fireice_client.exe 127.0.0.1 water
