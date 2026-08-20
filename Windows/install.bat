@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0.."

echo ===============================================================
echo                 SearchLeads CSV Manager Installer
echo ===============================================================
echo.

:: Check for built binaries in build\Windows\ directory
if not exist "build\Windows\slman.exe" (
    echo [ERROR] Could not find build\Windows\slman.exe!
    echo Please compile the project first by running Windows\build.bat.
    echo.
    if "%~1" neq "--quiet" pause
    exit /b 1
)

if not exist "build\Windows\slman_GUI.exe" (
    echo [ERROR] Could not find build\Windows\slman_GUI.exe!
    echo Please compile the project first by running Windows\build.bat.
    echo.
    if "%~1" neq "--quiet" pause
    exit /b 1
)

:: Set install target directory
set "INSTALL_DIR=%LOCALAPPDATA%\Programs\slman"

echo Installing to: "!INSTALL_DIR!"...
if not exist "!INSTALL_DIR!" mkdir "!INSTALL_DIR!"

:: Copy executables directly from build\Windows\ folder
copy /y "build\Windows\slman.exe" "!INSTALL_DIR!\slman.exe" >nul
copy /y "build\Windows\slman_GUI.exe" "!INSTALL_DIR!\slman_GUI.exe" >nul

:: Copy icon, uninstaller, and config
if exist "assets\app.ico" copy /y "assets\app.ico" "!INSTALL_DIR!\app.ico" >nul
if exist "Windows\uninstall.bat" copy /y "Windows\uninstall.bat" "!INSTALL_DIR!\uninstall.bat" >nul
if exist "mapping_config.json" (
    if not exist "!INSTALL_DIR!\mapping_config.json" (
        copy /y "mapping_config.json" "!INSTALL_DIR!\mapping_config.json" >nul
    )
)

:: Clean old shortcuts if exist
if exist "%USERPROFILE%\Desktop\SearchLeads CSV Manager.lnk" del /f /q "%USERPROFILE%\Desktop\SearchLeads CSV Manager.lnk" >nul 2>&1
if exist "%USERPROFILE%\Desktop\SLMAN GUI.lnk" del /f /q "%USERPROFILE%\Desktop\SLMAN GUI.lnk" >nul 2>&1

:: Create Desktop Shortcut for GUI via PowerShell
echo Creating Desktop Shortcut...
powershell -NoProfile -Command ^
    "$ws = New-Object -ComObject WScript.Shell;" ^
    "$s = $ws.CreateShortcut([System.IO.Path]::Combine([Environment]::GetFolderPath('Desktop'), 'slman GUI.lnk'));" ^
    "$s.TargetPath = '%LOCALAPPDATA%\Programs\slman\slman_GUI.exe';" ^
    "$s.WorkingDirectory = '%LOCALAPPDATA%\Programs\slman';" ^
    "$s.IconLocation = '%LOCALAPPDATA%\Programs\slman\app.ico,0';" ^
    "$s.Description = 'slman GUI';" ^
    "$s.Save()"

:: Create Start Menu Shortcuts via PowerShell
echo Creating Start Menu Shortcuts...
powershell -NoProfile -Command ^
    "$sm = [System.IO.Path]::Combine([Environment]::GetFolderPath('Programs'), 'slman');" ^
    "[void](New-Item -ItemType Directory -Path $sm -Force);" ^
    "$ws = New-Object -ComObject WScript.Shell;" ^
    "$s1 = $ws.CreateShortcut([System.IO.Path]::Combine($sm, 'slman GUI.lnk'));" ^
    "$s1.TargetPath = '%LOCALAPPDATA%\Programs\slman\slman_GUI.exe';" ^
    "$s1.WorkingDirectory = '%LOCALAPPDATA%\Programs\slman';" ^
    "$s1.IconLocation = '%LOCALAPPDATA%\Programs\slman\app.ico,0';" ^
    "$s1.Description = 'slman GUI';" ^
    "$s1.Save();" ^
    "$s2 = $ws.CreateShortcut([System.IO.Path]::Combine($sm, 'slman.lnk'));" ^
    "$s2.TargetPath = '%LOCALAPPDATA%\Programs\slman\slman.exe';" ^
    "$s2.WorkingDirectory = '%LOCALAPPDATA%\Programs\slman';" ^
    "$s2.IconLocation = '%LOCALAPPDATA%\Programs\slman\app.ico,0';" ^
    "$s2.Description = 'slman CLI';" ^
    "$s2.Save();" ^
    "$u = $ws.CreateShortcut([System.IO.Path]::Combine($sm, 'Uninstall.lnk'));" ^
    "$u.TargetPath = '%LOCALAPPDATA%\Programs\slman\uninstall.bat';" ^
    "$u.WorkingDirectory = '%LOCALAPPDATA%\Programs\slman';" ^
    "$u.IconLocation = '%LOCALAPPDATA%\Programs\slman\app.ico,0';" ^
    "$u.Description = 'Uninstall slman';" ^
    "$u.Save()"

:: Add to User PATH
echo Adding to User PATH environment variable...
powershell -NoProfile -Command ^
    "$installPath = '%LOCALAPPDATA%\Programs\slman';" ^
    "$userPath = [Environment]::GetEnvironmentVariable('Path', 'User');" ^
    "if ($userPath -notlike ('*' + $installPath + '*')) {" ^
    "    [Environment]::SetEnvironmentVariable('Path', ($userPath.TrimEnd(';') + ';' + $installPath), 'User');" ^
    "    Write-Host '  [OK] Added to PATH';" ^
    "} else {" ^
    "    Write-Host '  [OK] Already in PATH';" ^
    "}"

echo.
echo ===============================================================
echo Installation Complete!
echo.
echo Installed in:  !INSTALL_DIR!
echo Files:         slman.exe (CLI), slman_GUI.exe (GUI), app.ico, uninstall.bat
echo Shortcuts:     Desktop ('slman GUI') and Start Menu ('slman', 'slman GUI')
echo Command Line:  'slman' can now be run from any terminal!
echo ===============================================================
echo.
if "%~1" neq "--quiet" pause
