@echo off
setlocal
cd /d "%~dp0"

where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] cmake was not found in PATH.
    exit /b 1
)

cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++
if errorlevel 1 exit /b 1

cmake --build build --parallel 8
if errorlevel 1 exit /b 1

echo.
echo Built: %~dp0build\OpenGL_OBJ_Viewer.exe
