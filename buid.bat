@echo off
if not exist .log mkdir .log
set LOG_FILE=.log\build_windows.log

echo Build started at %date% %time% > "%LOG_FILE%"

echo 🧹 Cleaning old build directory...
echo 🧹 Cleaning old build directory... >> "%LOG_FILE%"
if exist build rmdir /s /q build
if %errorlevel% neq 0 exit /b %errorlevel%
mkdir build
if %errorlevel% neq 0 exit /b %errorlevel%

echo 🪟 Windows version is compiling...
echo 🪟 Windows version is compiling... >> "%LOG_FILE%"

cmake --preset windows-native
if %errorlevel% neq 0 exit /b %errorlevel%

cmake --build --preset windows
if %errorlevel% neq 0 exit /b %errorlevel%

cmake --install build/windows
if %errorlevel% neq 0 exit /b %errorlevel%

if not exist build\windows\.log mkdir build\windows\.log
copy /Y "%LOG_FILE%" build\windows\.log\build_windows.log >nul

echo Windows build done! Check 'build/windows' directory.
echo Windows build done! Check 'build/windows' directory. >> "%LOG_FILE%"