@echo off
setlocal
cd /d "%~dp0"

if exist "%LOCALAPPDATA%\Programs\slman\slman.exe" (
    "%LOCALAPPDATA%\Programs\slman\slman.exe" %*
) else if exist "build\slman.exe" (
    "build\slman.exe" %*
) else (
    echo [ERROR] slman.exe not found!
    echo Please build the project first by running build.bat.
    pause
)
