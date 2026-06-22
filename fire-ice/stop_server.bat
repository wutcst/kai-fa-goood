@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "BIN=%~dp0build\Release"
set "PID_FILE=%BIN%\fireice_server.pid"

if not exist "%PID_FILE%" (
    echo Server is not running ^(no pid file^).
    exit /b 0
)

set /p SERVER_PID=<"%PID_FILE%"
echo Stopping server ^(PID %SERVER_PID%^)...

%SystemRoot%\System32\taskkill.exe /PID %SERVER_PID% /T /F >nul 2>&1
if errorlevel 1 (
    echo Process %SERVER_PID% not found. Cleaning stale pid file.
) else (
    echo Server stopped.
)

del /f /q "%PID_FILE%" >nul 2>&1
exit /b 0
