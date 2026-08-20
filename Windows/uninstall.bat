@echo off
setlocal

:: Switch working directory to TEMP so the installation folder is not locked during deletion
cd /d "%TEMP%"

echo ===============================================================
echo             SearchLeads CSV Manager Uninstaller
echo ===============================================================
echo.

set "INSTALL_DIR=%LOCALAPPDATA%\Programs\slman"

echo Terminating running instances...
taskkill /F /IM slman_GUI.exe 2>nul
taskkill /F /IM slman.exe 2>nul
taskkill /F /IM sl_csv_manager.exe 2>nul

echo Removing installation directory: "%INSTALL_DIR%"...
if exist "%INSTALL_DIR%" (
    rmdir /S /Q "%INSTALL_DIR%"
)

:: Also clean old directories if present
if exist "%LOCALAPPDATA%\Programs\SL-CSV-Manager" (
    rmdir /S /Q "%LOCALAPPDATA%\Programs\SL-CSV-Manager" 2>nul
)

echo Removing Desktop Shortcuts...
powershell -NoProfile -Command ^
    "$desk1 = [System.IO.Path]::Combine([Environment]::GetFolderPath('Desktop'), 'SearchLeads CSV Manager.lnk');" ^
    "if (Test-Path $desk1) { Remove-Item $desk1 -Force };" ^
    "$desk2 = [System.IO.Path]::Combine([Environment]::GetFolderPath('Desktop'), 'SLMAN GUI.lnk');" ^
    "if (Test-Path $desk2) { Remove-Item $desk2 -Force };" ^
    "$desk3 = [System.IO.Path]::Combine([Environment]::GetFolderPath('Desktop'), 'slman.lnk');" ^
    "if (Test-Path $desk3) { Remove-Item $desk3 -Force };" ^
    "$desk4 = [System.IO.Path]::Combine([Environment]::GetFolderPath('Desktop'), 'SL CSV Manager.lnk');" ^
    "if (Test-Path $desk4) { Remove-Item $desk4 -Force }"

echo Removing Start Menu Shortcuts...
powershell -NoProfile -Command ^
    "$sm1 = [System.IO.Path]::Combine([Environment]::GetFolderPath('Programs'), 'SearchLeads CSV Manager');" ^
    "if (Test-Path $sm1) { Remove-Item $sm1 -Recurse -Force };" ^
    "$sm2 = [System.IO.Path]::Combine([Environment]::GetFolderPath('Programs'), 'slman');" ^
    "if (Test-Path $sm2) { Remove-Item $sm2 -Recurse -Force };" ^
    "$sm3 = [System.IO.Path]::Combine([Environment]::GetFolderPath('Programs'), 'SL CSV Manager');" ^
    "if (Test-Path $sm3) { Remove-Item $sm3 -Recurse -Force }"

echo Cleaning User PATH...
powershell -NoProfile -Command ^
    "$installPath = '%LOCALAPPDATA%\Programs\slman';" ^
    "$oldPath = '%LOCALAPPDATA%\Programs\SL-CSV-Manager';" ^
    "$userPath = [Environment]::GetEnvironmentVariable('Path', 'User');" ^
    "if ($userPath) {" ^
    "    $parts = $userPath.Split(';');" ^
    "    $filtered = @();" ^
    "    foreach ($p in $parts) { if ($p -and $p -ne $installPath -and $p -ne $oldPath) { $filtered += $p } };" ^
    "    $cleanPath = $filtered -join ';';" ^
    "    [Environment]::SetEnvironmentVariable('Path', $cleanPath, 'User');" ^
    "    Write-Host '  [OK] Removed from PATH';" ^
    "}"

echo.
echo ===============================================================
echo Uninstallation Complete!
echo ===============================================================
pause
