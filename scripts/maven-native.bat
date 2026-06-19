@echo off
setlocal

set ROOT=%~dp0..
set ACTION=%~1

if "%ACTION%"=="format" (
  call "%ROOT%\scripts\clang-format-check.bat"
  exit /b %ERRORLEVEL%
)
if "%ACTION%"=="build" (
  call "%ROOT%\scripts\build-native.bat"
  exit /b %ERRORLEVEL%
)
if "%ACTION%"=="test" (
  call "%ROOT%\scripts\run-native-tests.bat"
  exit /b %ERRORLEVEL%
)

echo Usage: %~nx0 {format^|build^|test}
exit /b 1
