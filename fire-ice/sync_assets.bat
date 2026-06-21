@echo off
setlocal

set "ROOT=%~dp0"
set "SRC=%ROOT%assets"
set "DST=%ROOT%build\Release"

if not exist "%DST%" (
    echo Build output not found. Please run build.bat first.
    exit /b 1
)

echo Syncing assets to build\Release ...

if exist "%SRC%\levels" (
    robocopy "%SRC%\levels" "%DST%\levels" /E /NFL /NDL /NJH /NJS /nc /ns /np >nul
)
if exist "%SRC%\textures" (
    robocopy "%SRC%\textures" "%DST%\textures" /E /NFL /NDL /NJH /NJS /nc /ns /np >nul
)
if exist "%SRC%\maps" (
    robocopy "%SRC%\maps" "%DST%\maps" /E /NFL /NDL /NJH /NJS /nc /ns /np >nul
)

echo Assets synced.
exit /b 0
