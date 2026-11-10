rem Written by VTSTech (Nigel Todman) (veritas@vts-tech.org)
rem AI Assisted by Super-Z (GLM-5.3-Flash)
rem License: MIT

@echo off
REM ============================================================
REM  install_git.cmd - one-time helper: install Git for Windows
REM  via winget, so install_shaders.cmd can do its git clone
REM  and git lfs pull. Git LFS is bundled with Git for Windows.
REM
REM  Equivalent manual command:
REM    winget install --id Git.Git -e --source winget
REM  To upgrade later:
REM    winget upgrade --id Git.Git -e
REM ============================================================

echo Checking for winget ...
set "WINGET_CMD="
where.exe winget >nul 2>nul && set "WINGET_CMD=winget"
if not defined WINGET_CMD if exist "%LOCALAPPDATA%\Microsoft\WindowsApps\winget.exe" set "WINGET_CMD=%LOCALAPPDATA%\Microsoft\WindowsApps\winget.exe"
if not defined WINGET_CMD (
    echo [ERROR] winget was not found on this system.
    echo         Open Microsoft Store, install/update "App Installer",
    echo         then re-run this script. Or install Git manually from:
    echo         https://github.com/git-for-windows/git/releases/latest
    pause
    exit /b 1
)

echo Checking for Git ...
where.exe git >nul 2>nul
if not errorlevel 1 (
    echo [OK] Git is already installed:
    git --version
    echo Nothing to do. You can run install_shaders.cmd
    pause
    exit /b 0
)

echo Installing Git for Windows (Git.Git) via winget ...
echo A UAC prompt may appear, please accept it.
"%WINGET_CMD%" install --id Git.Git -e --source winget --accept-source-agreements --accept-package-agreements
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] winget install failed.
    echo         Install Git manually from:
    echo         https://github.com/git-for-windows/git/releases/latest
    pause
    exit /b 1
)

REM Make git usable in THIS window without reopening it.
if exist "%ProgramFiles%\Git\cmd\git.exe" set "PATH=%ProgramFiles%\Git\cmd;%PATH%"
if exist "%ProgramFiles(x86)%\Git\cmd\git.exe" set "PATH=%ProgramFiles(x86)%\Git\cmd;%PATH%"
if exist "%LOCALAPPDATA%\Programs\Git\cmd\git.exe" set "PATH=%LOCALAPPDATA%\Programs\Git\cmd;%PATH%"

echo.
where.exe git >nul 2>nul
if errorlevel 1 (
    echo [WARN] Installed, but git.exe is not on PATH in this window yet.
    echo        Close and reopen your terminal, then run install_shaders.cmd
) else (
    echo [OK] Git installed:
    git --version
    echo Git LFS is bundled with Git for Windows.
    echo You can now run install_shaders.cmd
)
pause
exit /b 0
