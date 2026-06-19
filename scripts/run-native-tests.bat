@echo off
setlocal

set ROOT=%~dp0..
set BUILD=%ROOT%\fire-ice\build

if not exist "%BUILD%\fireice_tests.exe" (
  echo Missing fireice_tests.exe. Rebuild native project first.
  exit /b 1
)

echo Running native unit tests...
pushd "%BUILD%"
fireice_tests.exe
if %ERRORLEVEL% NEQ 0 (
  popd
  exit /b 1
)

if not exist "%BUILD%\fireice_server.exe" (
  echo Missing fireice_server.exe.
  popd
  exit /b 1
)

echo Running server smoke test...
start "" /B fireice_server.exe
timeout /t 3 /nobreak >nul
tasklist /FI "IMAGENAME eq fireice_server.exe" 2>nul | find /I "fireice_server.exe" >nul
if errorlevel 1 (
  echo Server exited prematurely during smoke test.
  popd
  exit /b 1
)

taskkill /IM fireice_server.exe /F >nul 2>&1
echo Server smoke test passed.
popd
exit /b 0
