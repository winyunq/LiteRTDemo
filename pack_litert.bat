@echo off
setlocal

set "PLUGIN_DIR=%~dp0Plugins\LiteRT-LM-Unreal"
if not exist "%PLUGIN_DIR%\export_fab.bat" (
    echo [ERROR] Packaging script not found at %PLUGIN_DIR%\export_fab.bat
    exit /b 1
)

echo [INFO] Starting LiteRT-LM-Unreal packaging...
call "%PLUGIN_DIR%\export_fab.bat" --cli %*
set "EXIT_CODE=%ERRORLEVEL%"

if not "%EXIT_CODE%"=="0" (
    echo [ERROR] Packaging failed with exit code %EXIT_CODE%.
    exit /b %EXIT_CODE%
)

echo [OK] Packaging complete.
exit /b 0
