@echo off
setlocal

set "ROOT=%~dp0.."
set "ACTION=%~1"

if "%ACTION%"=="format" goto run_format
if "%ACTION%"=="build" goto run_build
if "%ACTION%"=="test" goto run_test

echo Usage: %~nx0 {format^|build^|test}
exit /b 1

:run_format
call "%ROOT%\scripts\clang-format-check.bat"
exit /b %ERRORLEVEL%

:run_build
call "%ROOT%\scripts\build-native.bat"
exit /b %ERRORLEVEL%

:run_test
call "%ROOT%\scripts\run-native-tests.bat"
exit /b %ERRORLEVEL%
