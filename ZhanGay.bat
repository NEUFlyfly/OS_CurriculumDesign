@echo off
setlocal

title FlyflyUFS - ZhanGay Launcher

set "PROJECT_DIR=%~dp0"
set "MSYS2_UCRT64=C:\msys64\ucrt64"
set "CMAKE_EXE=%MSYS2_UCRT64%\bin\cmake.exe"
set "SERVER_EXE=%PROJECT_DIR%build\FlyflyUFS-server.exe"
set "FRONTEND_DIR=%PROJECT_DIR%frontend"
set "FRONTEND_URL=file:///%FRONTEND_DIR:\=/%/index.html"

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

:: Build
echo [1/3] Building...
cd /d "%PROJECT_DIR%"
"%CMAKE_EXE%" --preset msys2-ucrt64-debug
if errorlevel 1 ( pause & exit /b 1 )

"%CMAKE_EXE%" --build --preset msys2-ucrt64-debug --target FlyflyUFS-server
if errorlevel 1 ( pause & exit /b 1 )

echo [OK] Build done.
echo.

:: Start server
echo [2/3] Starting server...
taskkill /F /IM FlyflyUFS-server.exe >nul 2>&1
:: Delete data.img to ensure clean state (prevents corruption from previous crashes)
if exist "%PROJECT_DIR%data.img" del "%PROJECT_DIR%data.img"
start "FlyflyUFS Server" "%SERVER_EXE%"
timeout /t 2 /nobreak >nul
echo [OK] Server running.
echo.

:: Open frontend
echo [3/3] Opening browser...
start "" "%FRONTEND_URL%"
echo [OK] Done.
echo.
echo Server: ws://localhost:9001/ws
echo Login:  root / root
echo.
echo Press any key to stop server and exit...
pause >nul

taskkill /F /IM FlyflyUFS-server.exe >nul 2>&1
echo Stopped.
