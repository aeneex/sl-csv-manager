@echo off
setlocal
cd /d "%~dp0.."

:: Auto-detect and append common compiler locations to PATH
if exist "C:\msys64\ucrt64\bin" set "PATH=C:\msys64\ucrt64\bin;%PATH%"
if exist "C:\msys64\mingw64\bin" set "PATH=C:\msys64\mingw64\bin;%PATH%"
if exist "C:\Program Files\LLVM\bin" set "PATH=C:\Program Files\LLVM\bin;%PATH%"

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

:: Try Ninja generator first silently
cmake -B build\Windows -G "Ninja" -DCMAKE_BUILD_TYPE=Release >nul 2>&1
if %errorlevel% equ 0 goto :do_build

:: Try default generator
cmake -B build\Windows -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 goto :no_compiler

:do_build
cmake --build build\Windows --config Release
if %errorlevel% neq 0 goto :build_failed

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
exit /b 0

:no_compiler
echo.
echo ===============================================================
echo [ERROR] No C++ Compiler Found!
echo.
echo CMake requires a C++ compiler (MSVC, MinGW/GCC, or Clang).
echo.
echo To install a compiler, run one of the following commands:
echo.
echo   1. MinGW-w64 via WinLibs / MSYS2:
echo      winget install MSYS2.MSYS2
echo.
echo   2. Visual Studio 2022 Build Tools (MSVC):
echo      winget install Microsoft.VisualStudio.2022.BuildTools
echo.
echo   3. LLVM / Clang:
echo      winget install LLVM.LLVM
echo ===============================================================
echo.
pause
exit /b 1

:build_failed
echo.
echo [ERROR] Build failed!
pause
exit /b 1
