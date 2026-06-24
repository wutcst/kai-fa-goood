@echo off
setlocal
set "ROOT=%~dp0.."
if "%~1"=="" (
  python "%ROOT%\tools\bump_version.py" --read
  exit /b %ERRORLEVEL%
)
python "%ROOT%\tools\bump_version.py" %1
exit /b %ERRORLEVEL%
