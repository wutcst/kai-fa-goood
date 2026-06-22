@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "BIN=%~dp0build\Release"
set "PID_FILE=%BIN%\fireice_server.pid"
set "LOG_FILE=%BIN%\fireice_server.log"
set "EXE=%BIN%\fireice_server.exe"

if not exist "%EXE%" (
    echo Please run build.bat first.
    exit /b 1
)

if exist "%PID_FILE%" (
    set /p SERVER_PID=<"%PID_FILE%"
    %SystemRoot%\System32\tasklist.exe /FI "PID eq %SERVER_PID%" 2>nul | %SystemRoot%\System32\find.exe /I "fireice_server.exe" >nul
    if not errorlevel 1 (
        echo Server already running ^(PID %SERVER_PID%^).
        echo Log: %LOG_FILE%
        exit /b 0
    )
    del /f /q "%PID_FILE%" >nul 2>&1
)

echo Starting Fire-Ice server in background...
powershell -NoProfile -WindowStyle Hidden -Command ^
  "Start-Process -FilePath '%EXE%' -WorkingDirectory '%BIN%' -ArgumentList '--pid-file','%PID_FILE%','--log-file','%LOG_FILE%' -WindowStyle Hidden"

timeout /t 2 /nobreak >nul

if not exist "%PID_FILE%" (
    echo Failed to start server. Check %LOG_FILE%
    exit /b 1
)

set /p SERVER_PID=<"%PID_FILE%"
echo Server started ^(PID %SERVER_PID%^).
echo UDP port: 24567
echo Log: %LOG_FILE%
echo Stop with: stop_server.bat
