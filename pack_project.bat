@echo off
setlocal

:: Path to Unreal Engine 5.6
set "UE_ROOT=D:\UE_5.6"
set "UAT_PATH=%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat"

:: Project paths
set "PROJECT_PATH=%~dp0LiteRTDemo.uproject"
set "ARCHIVE_PATH=%~dp0Binaries\Packaged"

echo [INFO] Starting UE5 Project Packaging (Win64, Shipping)...
echo [INFO] Project: %PROJECT_PATH%
echo [INFO] Output:  %ARCHIVE_PATH%

if not exist "%UAT_PATH%" (
    echo [ERROR] RunUAT.bat not found at %UAT_PATH%
    exit /b 1
)

call "%UAT_PATH%" BuildCookRun ^
    -project="%PROJECT_PATH%" ^
    -targetplatform=Win64 ^
    -clientconfig=Shipping ^
    -cook ^
    -allmaps ^
    -build ^
    -stage ^
    -pak ^
    -archive ^
    -archivedirectory="%ARCHIVE_PATH%" ^
    -installed ^
    -utf8output

set "EXIT_CODE=%ERRORLEVEL%"

if "%EXIT_CODE%"=="0" (
    echo [OK] Packaging completed successfully.
    echo [INFO] You can find the packaged build in: %ARCHIVE_PATH%
) else (
    echo [ERROR] Packaging failed with exit code %EXIT_CODE%.
)

exit /b %EXIT_CODE%
