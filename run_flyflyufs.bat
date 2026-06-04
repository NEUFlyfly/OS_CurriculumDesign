@echo off
setlocal

set "PROJECT_DIR=%~dp0"
set "MSYS2_UCRT64=C:\msys64\ucrt64"
set "CMAKE_EXE=%MSYS2_UCRT64%\bin\cmake.exe"
set "APP_EXE=%PROJECT_DIR%build\FlyflyUFS.exe"
set "WEBUI_DIR=%PROJECT_DIR%webui"
set "WEBUI_URL=http://127.0.0.1:5173"
set "NO_PAUSE="
set "BUILD_ONLY="
set "NO_FRONTEND="
set "NO_BROWSER="
set "FRONTEND_ONLY="

:parse_args
if "%~1"=="" goto args_done
if "%~1"=="--no-pause" set "NO_PAUSE=1"
if "%~1"=="--build-only" set "BUILD_ONLY=1"
if "%~1"=="--no-frontend" set "NO_FRONTEND=1"
if "%~1"=="--no-browser" set "NO_BROWSER=1"
if "%~1"=="--frontend-only" set "FRONTEND_ONLY=1"
shift
goto parse_args
:args_done

cd /d "%PROJECT_DIR%"

if not exist "%CMAKE_EXE%" (
    echo [ERROR] CMake not found: %CMAKE_EXE%
    echo Please install MSYS2 UCRT64 packages first.
    echo See: Windows setup guide markdown in this project.
    if not defined NO_PAUSE pause
    exit /b 1
)

set "PATH=%MSYS2_UCRT64%\bin;%PATH%"
set "TERM=xterm"

echo [1/4] Configure FlyflyUFS...
"%CMAKE_EXE%" --preset msys2-ucrt64-debug
if errorlevel 1 (
    echo [ERROR] Configure failed.
    if not defined NO_PAUSE pause
    exit /b 1
)

echo.
echo [2/4] Build FlyflyUFS...
"%CMAKE_EXE%" --build --preset msys2-ucrt64-debug
if errorlevel 1 (
    echo [ERROR] Build failed.
    if not defined NO_PAUSE pause
    exit /b 1
)

if not exist "%APP_EXE%" (
    echo [ERROR] Executable not found: %APP_EXE%
    if not defined NO_PAUSE pause
    exit /b 1
)

if defined BUILD_ONLY (
    echo.
    echo Build completed: %APP_EXE%
    exit /b 0
)

if not defined NO_FRONTEND (
    echo.
    echo [3/4] Start web frontend...
    if not exist "%WEBUI_DIR%\index.html" (
        echo [WARN] Frontend entry not found: %WEBUI_DIR%\index.html
        echo [WARN] Skip frontend startup.
    ) else (
        where python >nul 2>nul
        if errorlevel 1 (
            echo [WARN] Python not found. Cannot start frontend static server.
            echo [WARN] Install Python or run this script from a Python-enabled terminal.
        ) else (
            powershell.exe -NoProfile -Command "if (Get-NetTCPConnection -LocalPort 5173 -State Listen -ErrorAction SilentlyContinue) { exit 0 } exit 1" >nul 2>nul
            if errorlevel 1 (
                start "FlyflyUFS Web UI Server" /min python -m http.server 5173 --directory "%WEBUI_DIR%"
                timeout /t 1 /nobreak >nul
            ) else (
                echo Frontend server already running: %WEBUI_URL%
            )
            if not defined NO_BROWSER start "" "%WEBUI_URL%"
            echo Frontend opened: %WEBUI_URL%
        )
    )
)

if defined FRONTEND_ONLY exit /b 0

echo.
echo [4/4] Run FlyflyUFS...
echo Default login: root / root
echo.
"%APP_EXE%"

echo.
echo FlyflyUFS exited.
if not defined NO_PAUSE pause
