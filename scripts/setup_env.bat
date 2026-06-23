@echo off
setlocal
echo === Fire-Ice 开发环境检查 ===
echo.

set "MISSING=0"

where git >nul 2>&1
if errorlevel 1 (echo [X] Git 未安装 & set MISSING=1) else (echo [OK] Git)

where cmake >nul 2>&1
if errorlevel 1 (
    if exist "C:\Program Files\CMake\bin\cmake.exe" (
        echo [OK] CMake ^(C:\Program Files\CMake\bin^)
        set "PATH=C:\Program Files\CMake\bin;%PATH%"
    ) else (
        echo [X] CMake 未安装
        set MISSING=1
    )
) else (
    echo [OK] CMake
)

if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    echo [OK] Visual Studio 2022 Build Tools
) else (
    echo [X] Visual Studio Build Tools 未安装
    set MISSING=1
)

echo.
if "%MISSING%"=="1" (
    echo 请先安装缺失组件，然后重新打开终端：
    echo   winget install -e --id Kitware.CMake
    echo   winget install -e --id Microsoft.VisualStudio.2022.BuildTools --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
    exit /b 1
)

echo 环境就绪。可执行：
echo   build.bat              编译项目
echo   start.bat              启动客户端
echo.
exit /b 0
