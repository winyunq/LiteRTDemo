param(
    [string]$ModelPath = 'D:\UE5Project\LiteRTDemo\Content\Models\gemma-4-E2B-it.litertlm',
    [string]$DllDirectory = 'D:\UE5Project\LiteRTDemo\Plugins\LiteRT-LM-Unreal\Source\ThirdParty\LiteRtLm\Binaries\Win64',
    [string]$OutputDirectory = 'D:\UE5Project\LiteRTDemo\Saved\Diagnostics\NativeProbe'
)

$ErrorActionPreference = 'Stop'
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = (Resolve-Path (Join-Path $Here '..\..')).Path
$Source = Join-Path $Here 'LiteRtLmConversationProbe.cpp'
$BinDirectory = Join-Path $Here 'bin'
$Executable = Join-Path $BinDirectory 'LiteRtLmConversationProbe.exe'
$IncludeDirectory = Join-Path $ProjectRoot 'Plugins\LiteRT-LM-Unreal\Source\ThirdParty\LiteRtLm\Include'

$VcVarsCandidates = @(
    'C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
)
$VcVars = $VcVarsCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $VcVars) {
    throw 'vcvars64.bat was not found.'
}
if (-not (Test-Path -LiteralPath $ModelPath)) {
    throw "Model not found: $ModelPath"
}
if (-not (Test-Path -LiteralPath (Join-Path $DllDirectory 'litert_lm_wrapper.dll'))) {
    throw "Wrapper DLL not found: $DllDirectory"
}

New-Item -ItemType Directory -Force -Path $BinDirectory | Out-Null
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

$CompileCommand = 'call "{0}" >nul && cl /nologo /EHsc /std:c++20 /utf-8 /I"{1}" "{2}" /Fe:"{3}"' -f $VcVars, $IncludeDirectory, $Source, $Executable
cmd.exe /d /s /c $CompileCommand
if ($LASTEXITCODE -ne 0) {
    throw "C++ compilation failed with exit code $LASTEXITCODE"
}

$Timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$LogPath = Join-Path $OutputDirectory "LiteRtLmConversationProbe-$Timestamp.jsonl"
$DllHash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $DllDirectory 'litert_lm_wrapper.dll')).Hash
Write-Host "Testing wrapper SHA256: $DllHash"
Write-Host "Model: $ModelPath"
Write-Host "Log: $LogPath"

& $Executable $ModelPath $DllDirectory $LogPath
exit $LASTEXITCODE
