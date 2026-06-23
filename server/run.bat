@echo off
REM 远端服务器部署脚本（本地玩家请用 start.bat 或 client\run.bat 仅启动客户端）
setlocal EnableExtensions

set "ROOT=%~dp0.."
cd /d "%ROOT%"

set "BIN=%ROOT%\build\Release"
set "PID_FILE=%BIN%\fireice_server.pid"
set "LOG_FILE=%BIN%\fireice_server.log"
set "EXE=%BIN%\fireice_server.exe"

if not exist "%EXE%" (
    echo Please run build.bat first.
    exit /b 1
)

%SystemRoot%\System32\tasklist.exe /FI "IMAGENAME eq fireice_server.exe" 2>nul | %SystemRoot%\System32\find.exe /I "fireice_server.exe" >nul
if not errorlevel 1 (
    echo Server already running.
    echo UDP port: 24567
    exit /b 0
)

if exist "%PID_FILE%" (
    del /f /q "%PID_FILE%" >nul 2>&1
)

echo Starting Fire-Ice server in background...
powershell -NoProfile -WindowStyle Hidden -Command ^
  "Start-Process -FilePath '%EXE%' -WorkingDirectory '%BIN%' -ArgumentList '--pid-file','%PID_FILE%','--log-file','%LOG_FILE%' -WindowStyle Hidden"

ping 127.0.0.1 -n 3 >nul

if exist "%PID_FILE%" (
    set /p SERVER_PID=<"%PID_FILE%"
    echo Server started ^(PID %SERVER_PID%^).
    echo UDP port: 24567
    echo Log: %LOG_FILE%
    echo Stop with: server\stop.bat
    exit /b 0
)

%SystemRoot%\System32\tasklist.exe /FI "IMAGENAME eq fireice_server.exe" 2>nul | %SystemRoot%\System32\find.exe /I "fireice_server.exe" >nul
if not errorlevel 1 (
    echo Server started ^(PID file pending^).
    echo UDP port: 24567
    exit /b 0
)

echo Failed to start server. Check %LOG_FILE%
exit /b 1
