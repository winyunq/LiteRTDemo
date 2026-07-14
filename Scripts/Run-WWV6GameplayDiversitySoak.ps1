[CmdletBinding()]
param(
    [string]$EngineRoot,
    [string]$ProjectPath,
    [ValidateRange(4096, 131072)]
    [int]$MinFreeVramMiB = 4800,
    [switch]$StaticSelfTest
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$ScriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$DefaultProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptDirectory ".."))
$TestMap = "/Game/WerewolfShowcase/Tests/L_WW_GameplayDiversitySoak"
$MinimumAcceptedSpeeches = 12

function Write-Step {
    param([string]$Message)

    Write-Host ""
    Write-Host ("==> " + $Message) -ForegroundColor Cyan
}

function Assert-File {
    param([string]$Path, [string]$Description)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw ("Missing {0}: {1}" -f $Description, $Path)
    }
}

function Resolve-ProjectFile {
    param([string]$RequestedPath)

    $Candidate = $RequestedPath
    if ([string]::IsNullOrWhiteSpace($Candidate)) {
        $Candidate = Join-Path $DefaultProjectRoot "LiteRTDemo.uproject"
    }

    $FullPath = [System.IO.Path]::GetFullPath($Candidate)
    Assert-File $FullPath "LiteRTDemo project"
    if ([System.IO.Path]::GetExtension($FullPath) -ine ".uproject") {
        throw ("ProjectPath must name a .uproject file: {0}" -f $FullPath)
    }
    return $FullPath
}

function Resolve-UnrealEngineRoot {
    param([string]$RequestedRoot)

    $Candidates = New-Object System.Collections.Generic.List[string]
    if (-not [string]::IsNullOrWhiteSpace($RequestedRoot)) {
        $Candidates.Add($RequestedRoot)
    }
    else {
        $Candidates.Add("D:\UE_5.8")
        $Candidates.Add("C:\Program Files\Epic Games\UE_5.8")
        $Candidates.Add("D:\Program Files\Epic Games\UE_5.8")
    }

    foreach ($Candidate in $Candidates) {
        if ([string]::IsNullOrWhiteSpace($Candidate)) {
            continue
        }
        $FullPath = [System.IO.Path]::GetFullPath($Candidate)
        $Editor = Join-Path $FullPath "Engine\Binaries\Win64\UnrealEditor.exe"
        $BuildVersion = Join-Path $FullPath "Engine\Build\Build.version"
        if (-not ((Test-Path -LiteralPath $Editor -PathType Leaf) -and
                  (Test-Path -LiteralPath $BuildVersion -PathType Leaf))) {
            continue
        }

        $Version = Get-Content -LiteralPath $BuildVersion -Raw | ConvertFrom-Json
        if ([int]$Version.MajorVersion -ne 5 -or [int]$Version.MinorVersion -ne 8) {
            throw ("The gameplay soak assets require UE 5.8, but {0} reports {1}.{2}." -f
                $FullPath, $Version.MajorVersion, $Version.MinorVersion)
        }
        return $FullPath
    }

    throw "UE 5.8 was not found. Pass -EngineRoot with the UE 5.8 installation directory."
}

function Get-BlockingUnrealProcesses {
    return @(
        Get-Process -ErrorAction SilentlyContinue |
            Where-Object {
                $_.ProcessName -ieq "UnrealEditor" -or
                $_.ProcessName -ieq "UnrealEditor-Cmd"
            } |
            Sort-Object ProcessName, Id
    )
}

function Assert-NoUnrealProcesses {
    param([string]$Checkpoint)

    $Blocking = @(Get-BlockingUnrealProcesses)
    if ($Blocking.Count -eq 0) {
        return
    }

    $Details = @(
        $Blocking | ForEach-Object {
            "{0}.exe PID={1}" -f $_.ProcessName, $_.Id
        }
    ) -join "; "
    throw ("Refusing GPU soak at {0}: close every existing UnrealEditor/UnrealEditor-Cmd process first. Found: {1}. No process was stopped." -f
        $Checkpoint, $Details)
}

function Resolve-NvidiaSmi {
    $Command = Get-Command "nvidia-smi.exe" -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($null -ne $Command) {
        return $Command.Source
    }

    $Candidates = @(
        (Join-Path $env:SystemRoot "System32\nvidia-smi.exe"),
        (Join-Path $env:ProgramFiles "NVIDIA Corporation\NVSMI\nvidia-smi.exe")
    )
    foreach ($Candidate in $Candidates) {
        if (Test-Path -LiteralPath $Candidate -PathType Leaf) {
            return $Candidate
        }
    }
    throw "nvidia-smi.exe was not found; NVIDIA GPU availability cannot be proven safely."
}

function Get-NvidiaGpuMemory {
    param([string]$NvidiaSmiPath)

    $Output = @(
        & $NvidiaSmiPath `
            "--query-gpu=index,memory.free,memory.total,name" `
            "--format=csv,noheader,nounits" 2>&1
    )
    $ExitCode = $LASTEXITCODE
    if ($ExitCode -ne 0) {
        throw ("nvidia-smi failed with exit code {0}: {1}" -f $ExitCode, ($Output -join " "))
    }

    $Devices = @()
    foreach ($Line in $Output) {
        $Text = [string]$Line
        if ([string]::IsNullOrWhiteSpace($Text)) {
            continue
        }
        $Parts = @($Text -split ",", 4)
        if ($Parts.Count -ne 4) {
            throw ("Unexpected nvidia-smi row: {0}" -f $Text)
        }
        [int]$Index = 0
        [int]$FreeMiB = 0
        [int]$TotalMiB = 0
        if (-not [int]::TryParse($Parts[0].Trim(), [ref]$Index) -or
            -not [int]::TryParse($Parts[1].Trim(), [ref]$FreeMiB) -or
            -not [int]::TryParse($Parts[2].Trim(), [ref]$TotalMiB)) {
            throw ("Could not parse nvidia-smi memory row: {0}" -f $Text)
        }
        $Devices += [pscustomobject]@{
            Index = $Index
            FreeMiB = $FreeMiB
            TotalMiB = $TotalMiB
            Name = $Parts[3].Trim()
        }
    }
    if ($Devices.Count -eq 0) {
        throw "nvidia-smi reported no NVIDIA GPUs."
    }
    return @($Devices | Sort-Object Index)
}

function Assert-ConservativeVram {
    param([object[]]$Devices, [int]$RequiredFreeMiB)

    # The project does not expose an adapter override, so the safest deterministic
    # check is adapter 0, which is the default selected by UE and the native delegate.
    $Primary = @($Devices | Sort-Object Index)[0]
    foreach ($Device in @($Devices)) {
        Write-Host ("NVIDIA GPU {0}: {1}; free {2} MiB / {3} MiB" -f
            $Device.Index, $Device.Name, $Device.FreeMiB, $Device.TotalMiB)
    }
    if ([int]$Primary.FreeMiB -lt $RequiredFreeMiB) {
        throw ("Refusing GPU soak: NVIDIA adapter 0 has {0} MiB free; at least {1} MiB is required. Close GPU-heavy applications and retry." -f
            $Primary.FreeMiB, $RequiredFreeMiB)
    }
    Write-Host ("VRAM gate passed: adapter 0 has {0} MiB free (minimum {1} MiB)." -f
        $Primary.FreeMiB, $RequiredFreeMiB) -ForegroundColor Green
}

function Get-DiagnosticsFileSet {
    param([string]$Directory)

    if (-not (Test-Path -LiteralPath $Directory -PathType Container)) {
        return @()
    }
    return @(
        Get-ChildItem -LiteralPath $Directory -Filter "*.jsonl" -File |
            ForEach-Object { [System.IO.Path]::GetFullPath($_.FullName) } |
            Sort-Object -Unique
    )
}

function Get-UniqueNewDiagnostic {
    param([string[]]$Before, [string[]]$After)

    $Known = New-Object "System.Collections.Generic.HashSet[string]" `
        ([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($Path in @($Before)) {
        [void]$Known.Add([System.IO.Path]::GetFullPath($Path))
    }
    $NewFiles = @(
        foreach ($Path in @($After)) {
            $FullPath = [System.IO.Path]::GetFullPath($Path)
            if (-not $Known.Contains($FullPath)) {
                $FullPath
            }
        }
    ) | Sort-Object -Unique

    if (@($NewFiles).Count -ne 1) {
        throw ("Expected exactly one new diagnostics JSONL, found {0}: {1}" -f
            @($NewFiles).Count, (@($NewFiles) -join "; "))
    }
    return @($NewFiles)[0]
}

function New-UnrealArguments {
    param([string]$ResolvedProjectPath, [string]$RuntimeLogPath)

    $Arguments = @(
        $ResolvedProjectPath,
        $TestMap,
        "-game",
        "-RenderOffscreen",
        "-windowed",
        "-ResX=1280",
        "-ResY=720",
        "-unattended",
        "-NoSplash",
        "-NoSound",
        "-stdout",
        "-FullStdOutLogOutput",
        ("-AbsLog=" + $RuntimeLogPath)
    )

    if (@($Arguments | Where-Object { $_ -match "(?i)^-?nullrhi(?:=|$)" }).Count -ne 0) {
        throw "Unsafe headless RHI suppression was detected in the launch arguments."
    }
    if (-not ($Arguments -ccontains "-RenderOffscreen")) {
        throw "RenderOffscreen is required for the GPU evidence run."
    }
    return $Arguments
}

function Resolve-PythonInvocation {
    param([string]$ResolvedEngineRoot)

    $Candidates = New-Object System.Collections.Generic.List[object]
    $SystemPython = Get-Command "python.exe" -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($null -ne $SystemPython) {
        $Candidates.Add([pscustomobject]@{ Executable = $SystemPython.Source; Prefix = @() })
    }
    $SystemPy = Get-Command "py.exe" -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($null -ne $SystemPy) {
        $Candidates.Add([pscustomobject]@{ Executable = $SystemPy.Source; Prefix = @("-3") })
    }
    $BundledPython = Join-Path $ResolvedEngineRoot "Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
    if (Test-Path -LiteralPath $BundledPython -PathType Leaf) {
        $Candidates.Add([pscustomobject]@{ Executable = $BundledPython; Prefix = @() })
    }

    foreach ($Candidate in $Candidates) {
        $ProbeArguments = @()
        $ProbeArguments += @($Candidate.Prefix)
        $ProbeArguments += @(
            "-c",
            "import sys; raise SystemExit(0 if sys.version_info >= (3, 9) else 1)"
        )
        & $Candidate.Executable @ProbeArguments *> $null
        if ($LASTEXITCODE -eq 0) {
            return $Candidate
        }
    }
    throw "Python 3.9 or newer was not found for the soak analyzer."
}

function Write-RunRecord {
    param([string]$Path, [hashtable]$Record)

    $Record["timestamp_utc"] = [DateTime]::UtcNow.ToString("o")
    $Json = $Record | ConvertTo-Json -Depth 6 -Compress
    Add-Content -LiteralPath $Path -Value $Json -Encoding UTF8
}

function Invoke-StaticSelfTest {
    $SyntheticRoot = [System.IO.Path]::GetFullPath((Join-Path $env:TEMP "wwv6-static"))
    $Args = @(New-UnrealArguments "C:\Synthetic\LiteRTDemo.uproject" (Join-Path $SyntheticRoot "run.log"))
    if ($Args[1] -cne $TestMap -or -not ($Args -ccontains "-RenderOffscreen")) {
        throw "Static self-test failed: launch contract drifted."
    }
    if (@($Args | Where-Object { $_ -match "(?i)nullrhi" }).Count -ne 0) {
        throw "Static self-test failed: RHI was disabled."
    }

    $Before = @("C:\Synthetic\old-a.jsonl", "C:\Synthetic\old-b.jsonl")
    $After = @($Before + "C:\Synthetic\new.jsonl")
    $Unique = Get-UniqueNewDiagnostic $Before $After
    if ($Unique -ine [System.IO.Path]::GetFullPath("C:\Synthetic\new.jsonl")) {
        throw "Static self-test failed: unique diagnostics pairing is incorrect."
    }
    $RejectedAmbiguity = $false
    try {
        [void](Get-UniqueNewDiagnostic $Before @($After + "C:\Synthetic\new-2.jsonl"))
    }
    catch {
        $RejectedAmbiguity = $true
    }
    if (-not $RejectedAmbiguity) {
        throw "Static self-test failed: ambiguous diagnostics were accepted."
    }
    Write-Host "WW_V6_GAMEPLAY_DIVERSITY_SOAK RUNNER_SELF_TEST PASS launch+RHI+diagnostics" -ForegroundColor Green
}

if ($StaticSelfTest) {
    Invoke-StaticSelfTest
    return
}

Write-Step "Resolve UE 5.8 project and test-only assets"
$ResolvedProjectPath = Resolve-ProjectFile $ProjectPath
$ProjectRoot = Split-Path -Parent $ResolvedProjectPath
$ResolvedEngineRoot = Resolve-UnrealEngineRoot $EngineRoot
$EditorExecutable = Join-Path $ResolvedEngineRoot "Engine\Binaries\Win64\UnrealEditor.exe"
$AnalyzerPath = Join-Path $ProjectRoot "Saved\WerewolfShowcaseTools\analyze_v6_gameplay_diversity_soak.py"
$DiagnosticsDirectory = Join-Path $ProjectRoot "Saved\Diagnostics"
$LogsDirectory = Join-Path $ProjectRoot "Saved\Logs"
$CoreAsset = Join-Path $ProjectRoot "Content\WerewolfShowcase\Core\BP_WW_GameCore.uasset"
$ModelAsset = Join-Path $ProjectRoot "Content\Models\gemma-4-E2B-it.litertlm"
$RequiredTestFiles = @(
    (Join-Path $ProjectRoot "Content\WerewolfShowcase\Tests\WBP_WW_GameplayDiversitySoak.uasset"),
    (Join-Path $ProjectRoot "Content\WerewolfShowcase\Tests\BP_WW_GameplayDiversitySoakGameMode.uasset"),
    (Join-Path $ProjectRoot "Content\WerewolfShowcase\Tests\L_WW_GameplayDiversitySoak.umap")
)
Assert-File $EditorExecutable "UnrealEditor GPU runtime"
Assert-File $AnalyzerPath "gameplay diversity analyzer"
Assert-File $CoreAsset "production GameCore evidence asset"
Assert-File $ModelAsset "LiteRT-LM model"
foreach ($TestFile in $RequiredTestFiles) {
    Assert-File $TestFile "generated test-only soak asset"
}
$Python = Resolve-PythonInvocation $ResolvedEngineRoot
New-Item -ItemType Directory -Path $DiagnosticsDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $LogsDirectory -Force | Out-Null

Write-Step "Reject competing Unreal processes"
Assert-NoUnrealProcesses "preflight"

Write-Step "Verify conservative NVIDIA VRAM headroom"
$NvidiaSmi = Resolve-NvidiaSmi
$GpuMemory = @(Get-NvidiaGpuMemory $NvidiaSmi)
Assert-ConservativeVram $GpuMemory $MinFreeVramMiB

Write-Step "Record the pre-run diagnostics set"
$RunId = "{0}-{1}" -f
    [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss-fff"),
    ([Guid]::NewGuid().ToString("N").Substring(0, 8))
$RuntimeLog = Join-Path $LogsDirectory ("WWV6GameplayDiversitySoak-{0}.log" -f $RunId)
$RunRecord = Join-Path $LogsDirectory ("WWV6GameplayDiversitySoak-run-{0}.jsonl" -f $RunId)
$DiagnosticsBefore = @(Get-DiagnosticsFileSet $DiagnosticsDirectory)
Write-RunRecord $RunRecord @{
    event = "preflight"
    run_id = $RunId
    project = $ResolvedProjectPath
    map = $TestMap
    runtime_log = $RuntimeLog
    min_free_vram_mib = $MinFreeVramMiB
    gpu_memory = $GpuMemory
    diagnostics_before = $DiagnosticsBefore
}
Write-Host ("Recorded {0} existing diagnostics files in {1}" -f
    $DiagnosticsBefore.Count, $RunRecord)

Write-Step "Launch the RenderOffscreen GPU gameplay soak"
$UnrealArguments = @(New-UnrealArguments $ResolvedProjectPath $RuntimeLog)
Assert-NoUnrealProcesses "immediately before launch"
Write-Host ("Runtime log: {0}" -f $RuntimeLog)
& $EditorExecutable @UnrealArguments
$UnrealExitCode = $LASTEXITCODE
Write-RunRecord $RunRecord @{
    event = "runtime_exited"
    run_id = $RunId
    unreal_exit_code = $UnrealExitCode
}

Write-Step "Select the one diagnostics JSONL created by this run"
$DiagnosticsAfter = @(Get-DiagnosticsFileSet $DiagnosticsDirectory)
$NewDiagnostic = Get-UniqueNewDiagnostic $DiagnosticsBefore $DiagnosticsAfter
Write-RunRecord $RunRecord @{
    event = "diagnostics_paired"
    run_id = $RunId
    diagnostics_after = $DiagnosticsAfter
    selected_diagnostics = $NewDiagnostic
}
Write-Host ("Selected diagnostics: {0}" -f $NewDiagnostic)

Write-Step "Run the metadata-only GPU diversity analyzer"
$AnalyzerArguments = @()
$AnalyzerArguments += @($Python.Prefix)
$AnalyzerArguments += @(
    $AnalyzerPath,
    "--ue-log", $RuntimeLog,
    "--diagnostics", $NewDiagnostic,
    "--core-asset", $CoreAsset,
    "--min-speeches", [string]$MinimumAcceptedSpeeches
)
& $Python.Executable @AnalyzerArguments
$AnalyzerExitCode = $LASTEXITCODE
Write-RunRecord $RunRecord @{
    event = "analyzer_exited"
    run_id = $RunId
    analyzer_exit_code = $AnalyzerExitCode
    unreal_exit_code = $UnrealExitCode
    selected_diagnostics = $NewDiagnostic
}
if ($AnalyzerExitCode -ne 0) {
    Write-Error ("Gameplay diversity analyzer failed with exit code {0}. Evidence was preserved in {1}." -f
        $AnalyzerExitCode, $RunRecord) -ErrorAction Continue
    exit $AnalyzerExitCode
}
if ($UnrealExitCode -ne 0) {
    throw ("UnrealEditor exited with code {0} even though the analyzer completed. Evidence: {1}" -f
        $UnrealExitCode, $RunRecord)
}

Write-Host ""
Write-Host "WW_V6_GAMEPLAY_DIVERSITY_SOAK RUNNER PASS" -ForegroundColor Green
Write-Host ("UE log:       {0}" -f $RuntimeLog)
Write-Host ("Diagnostics:  {0}" -f $NewDiagnostic)
Write-Host ("Run record:   {0}" -f $RunRecord)
