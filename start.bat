@echo off
setlocal

title FlyflyUFS - ZhanGay Launcher

set "PROJECT_DIR=%~dp0"
set "MSYS2_UCRT64=C:\msys64\ucrt64"
set "CMAKE_EXE=%MSYS2_UCRT64%\bin\cmake.exe"
set "SERVER_EXE=%PROJECT_DIR%build\FlyflyUFS-server.exe"
set "APP_EXE=%PROJECT_DIR%build\FlyflyUFS.exe"
set "FRONTEND_DIR=%PROJECT_DIR%frontend"
set "FRONTEND_URL=file:///%FRONTEND_DIR:\=/%/index.html"
set "SERVER_DEPS_OK=1"

echo.
echo ========================================
echo    FlyFS - ZhanGay
echo ========================================
echo.

:: Check CMake
if not exist "%CMAKE_EXE%" (
    echo [ERROR] CMake not found: %CMAKE_EXE%
    echo Install MSYS2 and run: pacman -S mingw-w64-ucrt-x86_64-cmake
    pause
    exit /b 1
)

set "PATH=%MSYS2_UCRT64%\bin;%PATH%"
set "TERM=xterm"

if not exist "%PROJECT_DIR%third_party\websocketpp_repo\websocketpp\config\asio_no_tls.hpp" set "SERVER_DEPS_OK=0"
if not exist "%PROJECT_DIR%third_party\json_repo\include\nlohmann\json.hpp" set "SERVER_DEPS_OK=0"

:: Build
echo [1/3] Building...
cd /d "%PROJECT_DIR%"
"%CMAKE_EXE%" --preset msys2-ucrt64-debug
if errorlevel 1 ( pause & exit /b 1 )

if "%SERVER_DEPS_OK%"=="1" (
    "%CMAKE_EXE%" --build --preset msys2-ucrt64-debug --target FlyflyUFS-server
    if errorlevel 1 ( pause & exit /b 1 )
) else (
    echo [WARN] WebSocket server dependencies are missing.
    echo [WARN] third_party\websocketpp_repo or third_party\json_repo is empty.
    echo [WARN] Build FlyflyUFS only and open frontend in mock mode.
    "%CMAKE_EXE%" --build --preset msys2-ucrt64-debug --target FlyflyUFS
    if errorlevel 1 ( pause & exit /b 1 )
)

echo [OK] Build done.
echo.

:: Start server
echo [2/3] Starting server...
if "%SERVER_DEPS_OK%"=="1" (
    taskkill /F /IM FlyflyUFS-server.exe >nul 2>&1
    start "FlyflyUFS Server" "%SERVER_EXE%"
    powershell.exe -NoProfile -Command "Start-Sleep -Seconds 2" >nul
    echo [OK] Server running.
) else (
    echo [WARN] Server skipped. Frontend will use mock mode.
)
echo.

:: Open frontend
echo [3/3] Opening browser...
start "" "%FRONTEND_URL%"
echo [OK] Done.
echo.
if "%SERVER_DEPS_OK%"=="1" echo Server: ws://localhost:9001/ws
if not "%SERVER_DEPS_OK%"=="1" echo Server: not started ^(mock mode^)
echo Login:  root / root
echo.
echo Press any key to stop server and exit...
pause >nul

if "%SERVER_DEPS_OK%"=="1" taskkill /F /IM FlyflyUFS-server.exe >nul 2>&1
echo Stopped.
