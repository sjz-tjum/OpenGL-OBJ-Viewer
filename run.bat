@echo off
setlocal
cd /d "%~dp0"

if not exist "build\OpenGL_OBJ_Viewer.exe" (
    call build.bat
    if errorlevel 1 exit /b 1
)

start "" "%~dp0build\OpenGL_OBJ_Viewer.exe"
