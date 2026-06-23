@echo off
setlocal enabledelayedexpansion

title FlyflyUFS - Environment Setup
cd /d "%~dp0"

:: ============================================================
::  FlyflyUFS 一键环境配置脚本
::  自动安装 MSYS2 + 所需 pacman 包 + git submodule
:: ============================================================

set "MSYS2_ROOT=C:\msys64"
set "MSYS2_BASH=%MSYS2_ROOT%\usr\bin\bash.exe"
set "MSYS2_PACMAN=%MSYS2_ROOT%\usr\bin\pacman.exe"
set "UCRT64_BIN=%MSYS2_ROOT%\ucrt64\bin"

:: ── 需要安装的 MSYS2 UCRT64 包 ──────────────────────────────
set "PACKAGES=mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-ncurses mingw-w64-ucrt-x86_64-tre mingw-w64-ucrt-x86_64-gettext mingw-w64-ucrt-x86_64-libiconv mingw-w64-ucrt-x86_64-pcre2 git"

echo.
echo ============================================================
echo   FlyflyUFS - Environment Setup
echo ============================================================
echo.

:: ============================================================
:: Step 0: Check Admin Permission (admin not required but recommended)
:: ============================================================
net session >nul 2>&1
if errorlevel 1 (
    echo [INFO] Not running as Administrator.
    echo        MSYS2 install may require admin rights.
    echo        If install fails, re-run as Administrator.
    echo.
)

:: ============================================================
:: Step 1: Install MSYS2
:: ============================================================
echo [1/4] Checking MSYS2...

if exist "%MSYS2_BASH%" (
    echo [OK] MSYS2 found at %MSYS2_ROOT%
    goto :msys2_ready
)

echo [WARN] MSYS2 not found at %MSYS2_ROOT%.
echo.
echo Attempting automatic installation...

:: Try winget first
where winget >nul 2>&1
if not errorlevel 1 (
    echo [INFO] Installing MSYS2 via winget...
    winget install --id MSYS2.MSYS2 --silent --accept-package-agreements --accept-source-agreements
    if exist "%MSYS2_BASH%" (
        echo [OK] MSYS2 installed via winget.
        goto :msys2_ready
    )
)

:: Fallback: manual download
echo.
echo [INFO] Automatic install failed. Opening MSYS2 download page...
echo        Please download from: https://www.msys2.org/
echo        Install to: %MSYS2_ROOT%
echo        After install, re-run this script.
start "" "https://github.com/msys2/msys2-installer/releases/latest"
pause
exit /b 1

:msys2_ready

:: ============================================================
:: Step 2: Update MSYS2 and install required packages
:: ============================================================
echo.
echo [2/4] Installing required packages...

:: Update package database and core packages first
echo   >>> Updating pacman database...
"%MSYS2_BASH%" -lc "pacman -Syu --noconfirm" 2>&1
:: pacman -Syu may self-update and ask to restart. Run once more to be safe.
"%MSYS2_BASH%" -lc "pacman -Syu --noconfirm" 2>&1

echo.
echo   >>> Installing UCRT64 packages...
"%MSYS2_BASH%" -lc "pacman -S --noconfirm --needed %PACKAGES%" 2>&1
if errorlevel 1 (
    echo [ERROR] Package installation failed.
    echo        Try running manually in MSYS2 UCRT64 terminal:
    echo          pacman -S --needed %PACKAGES%
    pause
    exit /b 1
)

echo [OK] Packages installed.

:: ============================================================
:: Step 3: Initialize/Update Git Submodules
:: ============================================================
echo.
echo [3/4] Setting up third-party libraries...

:: Check if git is available
where git >nul 2>&1
if errorlevel 1 (
    :: Try MSYS2 git
    if exist "%UCRT64_BIN%\git.exe" (
        set "PATH=%UCRT64_BIN%;%PATH%"
    ) else (
        echo [WARN] git not found. Skipping submodule init.
        echo        Install git and run: git submodule update --init --recursive
        goto :submodule_done
    )
)

:: Init submodules
echo   >>> Initializing git submodules...
git submodule update --init --recursive 2>&1
if errorlevel 1 (
    echo [WARN] Git submodule init had issues (maybe already initialized).
) else (
    echo [OK] Git submodules ready.
)

:submodule_done

:: Verify third_party contents
set "ALL_DEPS_OK=1"

if not exist "third_party\websocketpp_repo\websocketpp\config\asio_no_tls.hpp" (
    echo [WARN] websocketpp_repo is missing or incomplete.
    echo        Run: git submodule update --init --recursive
    set "ALL_DEPS_OK=0"
)

if not exist "third_party\json_repo\include\nlohmann\json.hpp" (
    echo [WARN] json_repo is missing or incomplete.
    echo        Run: git submodule update --init --recursive
    set "ALL_DEPS_OK=0"
)

if not exist "third_party\asio_repo\include\asio.hpp" (
    echo [WARN] asio_repo is missing asio.hpp.
    echo        Extract third_party\asio-1-12-2.zip into third_party\asio_repo\
    set "ALL_DEPS_OK=0"
)

:: ============================================================
:: Step 4: Verify and test-build
:: ============================================================
echo.
echo [4/4] Verifying environment...

:: Check each required executable
set "VERIFY_OK=1"

echo   Checking tools...
for %%E in (cmake.exe ninja.exe g++.exe) do (
    if exist "%UCRT64_BIN%\%%E" (
        echo     [OK] %%E
    ) else (
        echo     [MISS] %%E - not found in %UCRT64_BIN%
        set "VERIFY_OK=0"
    )
)

:: Check key library headers
echo   Checking C libraries...
set "UCRT64_INC=%MSYS2_ROOT%\ucrt64\include"

if exist "%UCRT64_INC%\ncursesw\ncurses.h" (
    echo     [OK] ncursesw
) else (
    echo     [MISS] ncursesw
    set "VERIFY_OK=0"
)

if exist "%UCRT64_INC%\tre\tre.h" (
    echo     [OK] tre
) else if exist "%UCRT64_INC%\tre.h" (
    echo     [OK] tre
) else (
    echo     [MISS] tre
    set "VERIFY_OK=0"
)

if exist "%MSYS2_ROOT%\ucrt64\lib\libintl.a" (
    echo     [OK] intl (gettext)
) else (
    echo     [MISS] intl
    set "VERIFY_OK=0"
)

if exist "%UCRT64_INC%\iconv.h" (
    echo     [OK] iconv
) else (
    echo     [MISS] iconv
    set "VERIFY_OK=0"
)

if exist "%MSYS2_ROOT%\ucrt64\lib\libpcre2-posix.a" (
    echo     [OK] pcre2-posix
) else (
    echo     [MISS] pcre2-posix
    set "VERIFY_OK=0"
)

echo.
if "!VERIFY_OK!"=="1" (
    echo ============================================================
    echo   [OK] All dependencies verified!
    echo ============================================================
    echo.
    echo   Next: Run start.bat to build and launch the application.
    echo.
) else (
    echo [WARN] Some components are missing.
    echo        Try running setup again, or install missing packages manually:
    echo        In MSYS2 UCRT64 terminal:
    echo          pacman -S --needed %PACKAGES%
    echo.
)

if "!ALL_DEPS_OK!"=="0" (
    echo [NOTE] Some third_party libraries are incomplete.
    echo        Run setup again or follow the [WARN] messages above.
    echo.
)

echo Press any key to exit...
pause >nul
endlocal
exit /b 0
