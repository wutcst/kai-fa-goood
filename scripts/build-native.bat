@echo off
setlocal

set ROOT=%~dp0..
set BUILD=%ROOT%\build

if exist "%BUILD%\CMakeCache.txt" (
  findstr /C:"CMAKE_GENERATOR:INTERNAL=Ninja" "%BUILD%\CMakeCache.txt" >nul
  if errorlevel 1 (
    echo Removing stale CMake cache...
    rmdir /s /q "%BUILD%"
  )
)

echo Configuring native build...
cmake -S "%ROOT%" -B "%BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% NEQ 0 (
  echo Native configure failed.
  exit /b 1
)

echo Building native binaries...
cmake --build "%BUILD%" --config Release --parallel
if %ERRORLEVEL% NEQ 0 (
  echo Native build failed.
  exit /b 1
)

if not exist "%BUILD%\fireice_server.exe" (
  echo Missing fireice_server.exe after build.
  exit /b 1
)

echo Native build completed.
exit /b 0
