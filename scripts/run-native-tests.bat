@echo off
setlocal

set "ROOT=%~dp0.."
set "BUILD=%ROOT%\build"

if not exist "%BUILD%\fireice_tests.exe" (
  echo Missing fireice_tests.exe. Rebuild native project first.
  exit /b 1
)

echo Running native unit tests...
pushd "%BUILD%"
fireice_tests.exe
if errorlevel 1 (
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
%SystemRoot%\System32\timeout.exe /t 3 /nobreak >nul
%SystemRoot%\System32\tasklist.exe /FI "IMAGENAME eq fireice_server.exe" 2>nul | %SystemRoot%\System32\find.exe /I "fireice_server.exe" >nul
if errorlevel 1 (
  echo Server exited prematurely during smoke test.
  popd
  exit /b 1
)

%SystemRoot%\System32\taskkill.exe /IM fireice_server.exe /F >nul 2>&1
echo Server smoke test passed.
popd
exit /b 0
