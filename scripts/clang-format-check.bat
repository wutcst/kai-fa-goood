@echo off
setlocal enabledelayedexpansion

set "ROOT=%~dp0.."
set "FAILED=0"
set "FILES=%TEMP%\fire-ice-format-files-%RANDOM%.txt"

echo Checking C++ source formatting...
set "CLANG_FMT=clang-format"
where clang-format >nul 2>&1
if errorlevel 1 (
  if exist "C:\Program Files\LLVM\bin\clang-format.exe" (
    set "CLANG_FMT=C:\Program Files\LLVM\bin\clang-format.exe"
  )
)
(
  dir /s /b "%ROOT%\client\src\*.cpp" "%ROOT%\client\src\*.hpp" 2>nul
  dir /s /b "%ROOT%\server\src\*.cpp" "%ROOT%\server\src\*.hpp" 2>nul
  dir /s /b "%ROOT%\shared\src\*.cpp" "%ROOT%\shared\src\*.hpp" 2>nul
) > "%FILES%" 2>nul

for /f "usebackq delims=" %%f in ("%FILES%") do (
  "%CLANG_FMT%" --dry-run --Werror "%%f" >nul 2>&1
  if errorlevel 1 (
    echo Formatting differs: %%f
    set "FAILED=1"
  )
)

del "%FILES%" >nul 2>&1

if "%FAILED%" NEQ "0" (
  echo Code formatting check failed.
  exit /b 1
)

echo All source files passed format check.
exit /b 0
