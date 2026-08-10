@echo off
setlocal
cd /d "%~dp0"

echo ===============================================================
echo            Configuring and Building SLMAN (CLI and GUI)
echo ===============================================================
echo.

cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo CMake Ninja generator failed. Trying default generator...
    cmake -B build -DCMAKE_BUILD_TYPE=Release
)

cmake --build build --config Release
if %errorlevel% equ 0 (
    taskkill /IM slman_GUI.exe /F >nul 2>&1
    taskkill /IM slman.exe /F >nul 2>&1
    echo.
    echo ===============================================================
    echo Build successful in build\ directory!
    echo Deploying to Programs environment...
    echo ===============================================================
    echo.
    call "%~dp0install.bat" --quiet
) else (
    echo.
    echo [ERROR] Build failed!
    pause
)
