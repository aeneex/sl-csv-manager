@echo off
setlocal
cd /d "%~dp0.."

echo ===============================================================
echo            Configuring and Building SLMAN (CLI and GUI)
echo ===============================================================
echo.

:: Terminate running processes to unlock binary files
taskkill /IM slman_GUI.exe /F >nul 2>&1
taskkill /IM slman.exe /F >nul 2>&1

:: Delete older versions in build directory before updating
if exist "build\Windows\slman.exe" del /f /q "build\Windows\slman.exe" >nul 2>&1
if exist "build\Windows\slman_GUI.exe" del /f /q "build\Windows\slman_GUI.exe" >nul 2>&1
if exist "build\Windows\Release\slman.exe" del /f /q "build\Windows\Release\slman.exe" >nul 2>&1
if exist "build\Windows\Release\slman_GUI.exe" del /f /q "build\Windows\Release\slman_GUI.exe" >nul 2>&1
if exist "build\Windows\Debug\slman.exe" del /f /q "build\Windows\Debug\slman.exe" >nul 2>&1
if exist "build\Windows\Debug\slman_GUI.exe" del /f /q "build\Windows\Debug\slman_GUI.exe" >nul 2>&1

cmake -B build\Windows -G "Ninja" -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo CMake Ninja generator failed. Trying default generator...
    cmake -B build\Windows -DCMAKE_BUILD_TYPE=Release
)

cmake --build build\Windows --config Release
if %errorlevel% equ 0 (
    :: Move compiled binaries to build\Windows\ root if placed in subdirectories by multi-config generator
    if exist "build\Windows\Release\slman.exe" move /y "build\Windows\Release\slman.exe" "build\Windows\" >nul 2>&1
    if exist "build\Windows\Release\slman_GUI.exe" move /y "build\Windows\Release\slman_GUI.exe" "build\Windows\" >nul 2>&1
    if exist "build\Windows\Debug\slman.exe" move /y "build\Windows\Debug\slman.exe" "build\Windows\" >nul 2>&1
    if exist "build\Windows\Debug\slman_GUI.exe" move /y "build\Windows\Debug\slman_GUI.exe" "build\Windows\" >nul 2>&1

    echo.
    echo ===============================================================
    echo Build successful in build\Windows\ directory!
    echo Deploying to Programs environment...
    echo ===============================================================
    echo.
    call "%~dp0install.bat" --quiet
) else (
    echo.
    echo [ERROR] Build failed!
    pause
)
