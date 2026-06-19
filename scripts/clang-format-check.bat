@echo off
setlocal enabledelayedexpansion

set ROOT=%~dp0..
set FAILED=0

echo Checking C++ source formatting...
for /f "delimiters=" %%f in ('dir /s /b "%ROOT%\fire-ice\src\*.cpp" "%ROOT%\fire-ice\src\*.hpp" 2^>nul') do (
  clang-format --dry-run --Werror "%%f" >nul 2>&1
  if errorlevel 1 (
    echo Formatting differs: %%f
    set FAILED=1
  )
)

if %FAILED% NEQ 0 (
  echo Code formatting check failed.
  exit /b 1
)

echo All source files passed format check.
exit /b 0
