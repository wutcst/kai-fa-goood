@echo off
setlocal

set "ROOT=%~dp0.."
cd /d "%ROOT%"

call "%ROOT%\build.bat"
if errorlevel 1 exit /b 1

set "SRC=build\Release"
set "DIST=dist\Fire-Ice-Online"

if exist "%DIST%" rmdir /S /Q "%DIST%"
mkdir "%DIST%"

echo Packaging client to %DIST% ...
xcopy /E /I /Y /Q "%SRC%\*" "%DIST%\" >nul
copy /Y "%SRC%\fireice_client.exe" "%DIST%\Fire-Ice Online.exe" >nul
del /F /Q "%DIST%\fireice_server.exe" >nul 2>&1
del /F /Q "%DIST%\fireice_server.log" >nul 2>&1
del /F /Q "%DIST%\fireice_server.pid" >nul 2>&1

(
echo Set shell = CreateObject^("WScript.Shell"^)
echo Set shortcut = shell.CreateShortcut^(shell.SpecialFolders^("Desktop"^) ^& "\Fire-Ice Online.lnk"^)
echo shortcut.TargetPath = "%CD%\%DIST%\Fire-Ice Online.exe"
echo shortcut.WorkingDirectory = "%CD%\%DIST%"
echo shortcut.Description = "Fire-Ice Online"
echo shortcut.Save
) > "%DIST%\create_desktop_shortcut.vbs"

echo.
echo Done. Double-click:
echo   %DIST%\Fire-Ice Online.exe
echo.
echo Optional desktop shortcut:
echo   wscript "%DIST%\create_desktop_shortcut.vbs"
echo.
pause
