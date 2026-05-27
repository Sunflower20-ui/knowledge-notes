@echo off
setlocal

set "QT_DIR=E:\zhuanye\QT"
set "CMAKE=%QT_DIR%\Tools\CMake_64\bin\cmake.exe"
set "NINJA=%QT_DIR%\Tools\Ninja\ninja.exe"
set "MINGW=%QT_DIR%\Tools\mingw1310_64\bin"
set "QT6=%QT_DIR%\6.11.0\mingw_64"

echo === KnowledgeNotes ? Day 1 Build [Qt 6.11.0 MinGW Debug] ===
echo.

echo [1/2] Configuring CMake...
"%CMAKE%" -S "%~dp0." -B "%~dp0build" -G "Ninja" ^
    -DCMAKE_PREFIX_PATH="%QT6%" ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_MAKE_PROGRAM="%NINJA%" ^
    -DCMAKE_C_COMPILER="%MINGW%\gcc.exe" ^
    -DCMAKE_CXX_COMPILER="%MINGW%\g++.exe"

if %ERRORLEVEL% NEQ 0 (
    echo CMake configure FAILED!
    pause
    exit /b 1
)

echo.
echo [2/2] Building...
"%CMAKE%" --build "%~dp0build"

if %ERRORLEVEL% NEQ 0 (
    echo Build FAILED!
    pause
    exit /b 1
)

echo.
echo === Build SUCCESS ===
echo Run: "%~dp0build\knowledge_notes.exe"
pause
