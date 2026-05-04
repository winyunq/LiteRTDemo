@echo off
setlocal

:: ============================================================
:: 1. Path Configuration - Using 'Saved' to avoid UE monitoring
:: ============================================================
set "PLUGIN_NAME=LiteRT-LM-Unreal"
set "ROOT_DIR=%~dp0"
set "PLUGIN_SRC=%ROOT_DIR%Plugins\%PLUGIN_NAME%"
:: We use a temporary staging area in 'Saved' which is ignored by UE file watchers
set "PACKAGE_ROOT=%ROOT_DIR%Saved\Packaged_Plugin"
set "OUTPUT_DIR=%PACKAGE_ROOT%\%PLUGIN_NAME%"

:: Generate Timestamp for ZIP name
for /f "tokens=2-4 delims=/ " %%a in ('date /t') do (set mydate=%%c%%a%%b)
for /f "tokens=1-2 delims=: " %%a in ('time /t') do (set mytime=%%a%%b)
set "TIMESTAMP=%mydate%_%mytime%"
set "ZIP_NAME=%PLUGIN_NAME%_v1.0_%TIMESTAMP%.zip"

echo [INFO] Surgical Packaging for %PLUGIN_NAME%...
echo [INFO] Staging Area: %OUTPUT_DIR%

:: ============================================================
:: 2. Manual Copyright Header Check
:: ============================================================
echo [INFO] Performing Manual Copyright Header Check...
set "MISSING_COPYRIGHT=0"
for /r "%PLUGIN_SRC%\Source" %%F in (*.h *.cpp *.cs) do (
    findstr /c:"Copyright" "%%F" >nul
    if errorlevel 1 (
        echo [WARN] Missing copyright header: %%F
        set "MISSING_COPYRIGHT=1"
    )
)

if "%MISSING_COPYRIGHT%"=="1" (
    echo [ERROR] Copyright check failed. Some files are missing headers.
    echo [ERROR] Aborting package process.
    pause
    exit /b 1
)
echo [OK] All source files contain copyright headers.

:: 3. Clean and recreate staging directory
if exist "%PACKAGE_ROOT%" (
    echo [INFO] Cleaning staging area...
    rd /s /q "%PACKAGE_ROOT%"
)
mkdir "%OUTPUT_DIR%"

:: 4. Generate a clean, Fab-compliant .uplugin file (Target 5.7.0)
echo [INFO] Generating Fab-compliant .uplugin (Target: 5.7.0)...
(
echo {
echo 	"FileVersion": 3,
echo 	"Version": 1,
echo 	"VersionName": "1.0",
echo 	"FriendlyName": "LiteRT-LM",
echo 	"Description": "High-performance, local LLM inference integration for Unreal Engine 5.",
echo 	"Category": "AI",
echo 	"CreatedBy": "Winyunq",
echo 	"CreatedByURL": "",
echo 	"DocsURL": "https://github.com/Winyunq/LiteRT-LM-Unreal",
echo 	"MarketplaceURL": "",
echo 	"SupportURL": "",
echo 	"EngineVersion": "5.7.0",
echo 	"CanContainContent": false,
echo 	"IsBetaVersion": true,
echo 	"IsExperimentalVersion": false,
echo 	"Installed": false,
echo 	"Modules": [
echo 		{
echo 			"Name": "LiteRTLMUnreal",
echo 			"Type": "Runtime",
echo 			"LoadingPhase": "PreDefault",
echo 			"PlatformAllowList": [
echo 				"Win64"
echo 			]
echo 		}
echo 	]
echo }
) > "%OUTPUT_DIR%\%PLUGIN_NAME%.uplugin"

:: 5. Ensure FilterPlugin.ini
echo [INFO] Ensuring FilterPlugin.ini...
if not exist "%OUTPUT_DIR%\Config" mkdir "%OUTPUT_DIR%\Config"
(
echo [FilterPlugin]
echo /Source/...
echo /Resources/...
echo /Config/...
) > "%OUTPUT_DIR%\Config\FilterPlugin.ini"

:: 6. Copy functional files (Surgical Copy)
echo [INFO] Copying functional files (.h, .cpp, .cs, .dll, .lib)...
robocopy "%PLUGIN_SRC%\Source" "%OUTPUT_DIR%\Source" *.h *.cpp *.cs *.dll *.lib /S /R:3 /W:5 >nul

:: Copy Resources (Icon, etc.)
if exist "%PLUGIN_SRC%\Resources" (
    echo [INFO] Copying resources...
    robocopy "%PLUGIN_SRC%\Resources" "%OUTPUT_DIR%\Resources" /S /R:3 /W:5 >nul
)

:: 7. COMPRESSION - Using tar.exe (Included in Win10/11)
:: Tar is significantly more robust against temporary file locks than PowerShell.
echo [INFO] Compressing to %ZIP_NAME%...
pushd "%PACKAGE_ROOT%"
tar -a -c -f "%ZIP_NAME%" "%PLUGIN_NAME%"
popd

:: 8. OUTPUT FAB TECHNICAL DETAILS TO CONSOLE
echo.
echo ============================================================
echo        FAB TECHNICAL DETAILS (COPY FROM BELOW)
echo ============================================================
echo Features:
echo 1. High-performance local LLM inference: Using LiteRT (TensorFlow Lite) backend.
echo 2. UE5 Optimized: PreDefault loading phase for seamless AI integration.
echo 3. High-performance C++ wrapper: Optimized for LLM tasks on Win64.
echo 4. Minimal footprint: Direct access to local models without cloud dependency.
echo 5. Production-ready: Designed for AI features in games and interactive applications.
echo.
echo Code Modules: LiteRTLMUnreal (Runtime)
echo Number of Blueprints: 0
echo Number of C++ Classes: 4
echo Network Replicated: No
echo Supported Development Platforms: Windows (Yes), Mac (No)
echo Supported Target Build Platforms: Win64
echo Documentation Link: [https://winyunq.github.io/LiteRT-LM-Unreal/index.html]
echo Example Project: N/A
echo Important/Additional Notes: Requires compatible LiteRT runtime libraries for inference.
echo ============================================================
echo.

if exist "%PACKAGE_ROOT%\%ZIP_NAME%" (
    echo [OK] Surgical package complete and zipped.
    echo [INFO] Final ZIP: %PACKAGE_ROOT%\%ZIP_NAME%
) else (
    echo [ERROR] ZIP compression failed. Check Staging Area at: %OUTPUT_DIR%
)

pause
exit /b 0
