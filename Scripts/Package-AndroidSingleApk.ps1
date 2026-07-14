[CmdletBinding()]
param(
    [string]$EngineRoot,
    [string]$AndroidSdkRoot,
    [string]$JavaHome,
    [ValidateSet("Development", "Shipping", "Test")]
    [string]$Configuration = "Development",
    [string]$ArchiveDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$ScriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $ScriptDirectory ".."))
$ProjectFile = Join-Path $ProjectRoot "LiteRTDemo.uproject"
$EngineConfig = Join-Path $ProjectRoot "Config\DefaultEngine.ini"
$AndroidPlugin = Join-Path $ProjectRoot "Plugins\LiteRT-LM-Unreal\Source\LiteRTLMUnreal\LiteRTLMUnreal_APL.xml"
$ModelFileName = "gemma-4-E2B-it.litertlm"
$ModelPath = Join-Path $ProjectRoot ("Content\Models\" + $ModelFileName)
$ChunkDirectory = Join-Path $ProjectRoot "Build\Android\LiteRtLmModelChunks"
$ChunkManifestPath = Join-Path $ChunkDirectory ($ModelFileName + ".parts")
$RequiredNdkVersion = "27.2.12479018"
$ModelChunkBytes = 640MB

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

function Assert-Directory {
    param([string]$Path, [string]$Description)
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw ("Missing {0}: {1}" -f $Description, $Path)
    }
}

function Resolve-EngineRoot {
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
        $BuildVersionPath = Join-Path $FullPath "Engine\Build\Build.version"
        $BuildBatchPath = Join-Path $FullPath "Engine\Build\BatchFiles\Build.bat"
        $RunUatPath = Join-Path $FullPath "Engine\Build\BatchFiles\RunUAT.bat"
        if ((Test-Path -LiteralPath $BuildVersionPath -PathType Leaf) -and
            (Test-Path -LiteralPath $BuildBatchPath -PathType Leaf) -and
            (Test-Path -LiteralPath $RunUatPath -PathType Leaf)) {
            return $FullPath
        }
    }

    throw "UE 5.8 was not found. Pass -EngineRoot with the UE 5.8 installation directory."
}

function Test-SdkRoot {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $false
    }

    $FullPath = [System.IO.Path]::GetFullPath($Path)
    return (Test-Path -LiteralPath (Join-Path $FullPath "build-tools") -PathType Container) -and
        (Test-Path -LiteralPath (Join-Path $FullPath "platforms") -PathType Container) -and
        (Test-Path -LiteralPath (Join-Path $FullPath ("ndk\" + $RequiredNdkVersion + "\source.properties")) -PathType Leaf)
}

function Resolve-AndroidSdkRoot {
    param([string]$RequestedRoot)

    $Candidates = New-Object System.Collections.Generic.List[string]
    if (-not [string]::IsNullOrWhiteSpace($RequestedRoot)) {
        $Candidates.Add($RequestedRoot)
    }
    else {
        $Candidates.Add("D:\SDK")
        $Candidates.Add([Environment]::GetEnvironmentVariable("ANDROID_SDK_ROOT", "Process"))
        $Candidates.Add([Environment]::GetEnvironmentVariable("ANDROID_HOME", "Process"))
        if (-not [string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
            $Candidates.Add((Join-Path $env:LOCALAPPDATA "Android\Sdk"))
        }
    }

    foreach ($Candidate in $Candidates) {
        if (Test-SdkRoot $Candidate) {
            return [System.IO.Path]::GetFullPath($Candidate)
        }
    }

    throw ("Android SDK with NDK r27c ({0}) was not found. Pass -AndroidSdkRoot." -f $RequiredNdkVersion)
}

function Resolve-JavaHome {
    param([string]$RequestedHome, [string]$ResolvedEngineRoot)

    $Candidates = New-Object System.Collections.Generic.List[string]
    if (-not [string]::IsNullOrWhiteSpace($RequestedHome)) {
        $Candidates.Add($RequestedHome)
    }
    else {
        $Candidates.Add([Environment]::GetEnvironmentVariable("JAVA_HOME", "Process"))
        $Candidates.Add("C:\Program Files\Android\Android Studio\jbr")
        $Candidates.Add((Join-Path $ResolvedEngineRoot "Engine\Extras\ThirdPartyNotUE\Android\JDK"))
    }

    foreach ($Candidate in $Candidates) {
        if ([string]::IsNullOrWhiteSpace($Candidate)) {
            continue
        }

        $FullPath = [System.IO.Path]::GetFullPath($Candidate)
        if (Test-Path -LiteralPath (Join-Path $FullPath "bin\java.exe") -PathType Leaf) {
            return $FullPath
        }
    }

    throw "A JDK was not found. Pass -JavaHome (Android Studio jbr is supported)."
}

function Find-AndroidBuildTools {
    param([string]$ResolvedSdkRoot)

    $Root = Join-Path $ResolvedSdkRoot "build-tools"
    $Candidates = @(
        Get-ChildItem -LiteralPath $Root -Directory |
            Where-Object {
                (Test-Path -LiteralPath (Join-Path $_.FullName "aapt2.exe") -PathType Leaf) -and
                (Test-Path -LiteralPath (Join-Path $_.FullName "apksigner.bat") -PathType Leaf)
            } |
            Sort-Object {
                $Parsed = New-Object System.Version
                if ([System.Version]::TryParse($_.Name, [ref]$Parsed)) {
                    return $Parsed
                }
                return [System.Version]::new(0, 0)
            } -Descending
    )

    if ($Candidates.Count -eq 0) {
        throw ("No Android build-tools containing aapt2.exe and apksigner.bat were found below {0}" -f $Root)
    }

    return $Candidates[0].FullName
}

function Get-IniValue {
    param(
        [string]$Path,
        [string]$Section,
        [string]$Key
    )

    $CurrentSection = ""
    $Value = $null
    foreach ($Line in Get-Content -LiteralPath $Path) {
        $Trimmed = $Line.Trim()
        if ($Trimmed -match '^\[(.+)\]$') {
            $CurrentSection = $Matches[1]
            continue
        }

        if (-not $CurrentSection.Equals($Section, [System.StringComparison]::OrdinalIgnoreCase)) {
            continue
        }

        $Match = [regex]::Match($Trimmed, ('^' + [regex]::Escape($Key) + '\s*=\s*(.*)$'), [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
        if ($Match.Success) {
            $Value = $Match.Groups[1].Value.Trim()
        }
    }

    if ($null -eq $Value) {
        throw ("Missing [{0}] {1} in {2}" -f $Section, $Key, $Path)
    }

    return $Value
}

function Invoke-NativeCommand {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    $DisplayArguments = @(
        foreach ($Argument in $Arguments) {
            if ($Argument -match '\s') {
                '"' + $Argument.Replace('"', '\"') + '"'
            }
            else {
                $Argument
            }
        }
    )
    Write-Host ("> {0} {1}" -f $FilePath, ($DisplayArguments -join " ")) -ForegroundColor DarkGray
    & $FilePath @Arguments
    $ExitCode = $LASTEXITCODE
    if ($ExitCode -ne 0) {
        throw ("Command failed with exit code {0}: {1}" -f $ExitCode, $FilePath)
    }
}

function Invoke-NativeCommandCapture {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    $Output = @(& $FilePath @Arguments 2>&1)
    $ExitCode = $LASTEXITCODE
    if ($ExitCode -ne 0) {
        $Text = ($Output | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine
        throw ("Command failed with exit code {0}: {1}`n{2}" -f $ExitCode, $FilePath, $Text)
    }

    return @($Output | ForEach-Object { $_.ToString() })
}

function Get-ChunkLayout {
    param(
        [string]$ManifestPath,
        [string]$ChunksPath,
        [string]$SourceModelPath,
        [string]$SourceModelFileName
    )

    Assert-File $ManifestPath "LiteRT-LM chunk manifest"
    $Lines = @(Get-Content -LiteralPath $ManifestPath | ForEach-Object { $_.Trim() } | Where-Object { $_.Length -gt 0 })
    if ($Lines.Count -ne 3) {
        throw ("Chunk manifest must contain count, byte length, and SHA-1: {0}" -f $ManifestPath)
    }

    [int]$ChunkCount = 0
    [long]$ManifestBytes = 0
    if (-not [int]::TryParse($Lines[0], [ref]$ChunkCount) -or $ChunkCount -lt 1 -or $ChunkCount -gt 64) {
        throw ("Invalid chunk count in manifest: {0}" -f $Lines[0])
    }
    if (-not [long]::TryParse($Lines[1], [ref]$ManifestBytes) -or $ManifestBytes -lt 1) {
        throw ("Invalid model byte count in manifest: {0}" -f $Lines[1])
    }
    $ManifestSha1 = $Lines[2].ToUpperInvariant()
    if ($ManifestSha1 -notmatch '^[0-9A-F]{40}$') {
        throw ("Invalid model SHA-1 in manifest: {0}" -f $Lines[2])
    }

    $ModelBytes = (Get-Item -LiteralPath $SourceModelPath).Length
    if ($ManifestBytes -ne $ModelBytes) {
        throw ("Manifest byte count ({0}) does not match model ({1})." -f $ManifestBytes, $ModelBytes)
    }

    $Files = New-Object System.Collections.Generic.List[System.IO.FileInfo]
    [long]$ChunkBytes = 0
    for ($Index = 0; $Index -lt $ChunkCount; ++$Index) {
        $Name = $SourceModelFileName + ".part" + $Index.ToString("D3") + ".png"
        $Path = Join-Path $ChunksPath $Name
        Assert-File $Path ("model chunk " + $Name)
        $Item = Get-Item -LiteralPath $Path
        $Files.Add($Item)
        $ChunkBytes += $Item.Length
    }

    $ActualFiles = @(Get-ChildItem -LiteralPath $ChunksPath -File -Filter ($SourceModelFileName + ".part*.png"))
    if ($ActualFiles.Count -ne $ChunkCount) {
        throw ("Expected exactly {0} model chunks, found {1}." -f $ChunkCount, $ActualFiles.Count)
    }
    if ($ChunkBytes -ne $ModelBytes) {
        throw ("Chunk byte sum ({0}) does not match model ({1})." -f $ChunkBytes, $ModelBytes)
    }

    return [pscustomobject]@{
        Count = $ChunkCount
        ManifestBytes = $ManifestBytes
        TotalBytes = $ChunkBytes
        Sha1 = $ManifestSha1
        Files = @($Files)
        ManifestText = ($Lines -join "`n") + "`n"
    }
}

function Write-ModelChunks {
    param(
        [string]$SourceModelPath,
        [string]$ChunksPath,
        [string]$SourceModelFileName,
        [long]$BytesPerChunk
    )

    if ($BytesPerChunk -lt 1) {
        throw "Model chunk size must be positive."
    }
    if (-not (Test-Path -LiteralPath $ChunksPath -PathType Container)) {
        New-Item -ItemType Directory -Path $ChunksPath -Force | Out-Null
    }

    $ExpectedFiles = New-Object System.Collections.Generic.HashSet[string]([System.StringComparer]::OrdinalIgnoreCase)
    $Buffer = New-Object byte[] (4MB)
    $Hasher = [System.Security.Cryptography.SHA1]::Create()
    $Reader = [System.IO.File]::Open($SourceModelPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
    $HashBytes = $null
    [long]$TotalBytes = 0
    [int]$ChunkIndex = 0
    try {
        while ($Reader.Position -lt $Reader.Length) {
            $ChunkName = $SourceModelFileName + ".part" + $ChunkIndex.ToString("D3") + ".png"
            $ExpectedFiles.Add($ChunkName) | Out-Null
            $ChunkPath = Join-Path $ChunksPath $ChunkName
            $Writer = [System.IO.File]::Open($ChunkPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
            [long]$ChunkWritten = 0
            try {
                while ($ChunkWritten -lt $BytesPerChunk -and $Reader.Position -lt $Reader.Length) {
                    $Requested = [int][Math]::Min([long]$Buffer.Length, $BytesPerChunk - $ChunkWritten)
                    $Read = $Reader.Read($Buffer, 0, $Requested)
                    if ($Read -le 0) {
                        throw "Unexpected end of model while generating chunks."
                    }
                    $Hasher.TransformBlock($Buffer, 0, $Read, $null, 0) | Out-Null
                    $Writer.Write($Buffer, 0, $Read)
                    $ChunkWritten += $Read
                    $TotalBytes += $Read
                }
            }
            finally {
                $Writer.Dispose()
            }
            ++$ChunkIndex
        }
        $Hasher.TransformFinalBlock([byte[]]::new(0), 0, 0) | Out-Null
        $HashBytes = $Hasher.Hash
    }
    finally {
        $Reader.Dispose()
        $Hasher.Dispose()
    }

    if ($TotalBytes -ne (Get-Item -LiteralPath $SourceModelPath).Length -or $ChunkIndex -lt 1) {
        throw "Generated model chunks do not match the source model length."
    }
    $Sha1 = [System.BitConverter]::ToString($HashBytes).Replace("-", "")
    $ManifestName = $SourceModelFileName + ".parts"
    $ExpectedFiles.Add($ManifestName) | Out-Null
    $ManifestPath = Join-Path $ChunksPath $ManifestName
    $ManifestText = ("{0}`n{1}`n{2}`n" -f $ChunkIndex, $TotalBytes, $Sha1)
    [System.IO.File]::WriteAllText($ManifestPath, $ManifestText, [System.Text.UTF8Encoding]::new($false))

    foreach ($Existing in @(Get-ChildItem -LiteralPath $ChunksPath -File)) {
        if (-not $ExpectedFiles.Contains($Existing.Name)) {
            Remove-Item -LiteralPath $Existing.FullName -Force
        }
    }
}

function Test-IsStrictChildPath {
    param([string]$Parent, [string]$Child)

    $ParentFull = [System.IO.Path]::GetFullPath($Parent).TrimEnd([char[]]@('\', '/'))
    $ChildFull = [System.IO.Path]::GetFullPath($Child).TrimEnd([char[]]@('\', '/'))
    $Prefix = $ParentFull + [System.IO.Path]::DirectorySeparatorChar
    return $ChildFull.StartsWith($Prefix, [System.StringComparison]::OrdinalIgnoreCase)
}

function Assert-NoReparsePointBelowRoot {
    param(
        [string]$Root,
        [string]$Path
    )

    $RootFull = [System.IO.Path]::GetFullPath($Root).TrimEnd([char[]]@('\', '/'))
    $Current = [System.IO.Path]::GetFullPath($Path).TrimEnd([char[]]@('\', '/'))
    while (-not $Current.Equals($RootFull, [System.StringComparison]::OrdinalIgnoreCase)) {
        if (-not (Test-IsStrictChildPath $RootFull $Current)) {
            throw ("Path escaped the workspace while checking reparse points: {0}" -f $Current)
        }
        if (Test-Path -LiteralPath $Current) {
            $CurrentItem = Get-Item -LiteralPath $Current -Force
            if (($CurrentItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw ("Refusing to recursively clean through a reparse point: {0}" -f $Current)
            }
        }

        $Parent = [System.IO.Path]::GetDirectoryName($Current)
        if ([string]::IsNullOrWhiteSpace($Parent) -or $Parent.Equals($Current, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw ("Could not safely walk cleanup path back to workspace root: {0}" -f $Path)
        }
        $Current = $Parent.TrimEnd([char[]]@('\', '/'))
    }
}

function Remove-ApprovedWorkspaceDirectory {
    param(
        [string]$Path,
        [string[]]$ApprovedPaths
    )

    $FullPath = [System.IO.Path]::GetFullPath($Path).TrimEnd([char[]]@('\', '/'))
    $IsApproved = $false
    foreach ($ApprovedPath in $ApprovedPaths) {
        $ApprovedFull = [System.IO.Path]::GetFullPath($ApprovedPath).TrimEnd([char[]]@('\', '/'))
        if ($FullPath.Equals($ApprovedFull, [System.StringComparison]::OrdinalIgnoreCase)) {
            $IsApproved = $true
            break
        }
    }

    if (-not $IsApproved -or -not (Test-IsStrictChildPath $ProjectRoot $FullPath)) {
        throw ("Refusing to clean a path outside the exact workspace allow-list: {0}" -f $FullPath)
    }

    Assert-NoReparsePointBelowRoot $ProjectRoot $FullPath
    if (Test-Path -LiteralPath $FullPath) {
        Write-Host ("Removing stale Android output: " + $FullPath)
        Remove-Item -LiteralPath $FullPath -Recurse -Force
    }
}

function Normalize-ArchiveEntryName {
    param([string]$Name)
    $Normalized = $Name.Trim().Replace('\', '/')
    while ($Normalized.StartsWith("./", [System.StringComparison]::Ordinal)) {
        $Normalized = $Normalized.Substring(2)
    }
    return $Normalized.TrimStart('/')
}

Write-Step "Validate project, UE 5.8, Android SDK/NDK r27c, JDK, and model"
Assert-File $ProjectFile "project file"
Assert-File $EngineConfig "DefaultEngine.ini"
Assert-File $AndroidPlugin "LiteRT-LM Android plugin XML"
Assert-File $ModelPath "LiteRT-LM model"

$ResolvedEngineRoot = Resolve-EngineRoot $EngineRoot
$BuildVersionPath = Join-Path $ResolvedEngineRoot "Engine\Build\Build.version"
$BuildVersion = Get-Content -LiteralPath $BuildVersionPath -Raw | ConvertFrom-Json
if ([int]$BuildVersion.MajorVersion -ne 5 -or [int]$BuildVersion.MinorVersion -ne 8) {
    throw ("This runbook requires UE 5.8; found {0}.{1} at {2}." -f $BuildVersion.MajorVersion, $BuildVersion.MinorVersion, $ResolvedEngineRoot)
}

$ResolvedSdkRoot = Resolve-AndroidSdkRoot $AndroidSdkRoot
$ResolvedNdkRoot = Join-Path $ResolvedSdkRoot ("ndk\" + $RequiredNdkVersion)
$NdkProperties = Get-Content -LiteralPath (Join-Path $ResolvedNdkRoot "source.properties") -Raw
if ($NdkProperties -notmatch '(?m)^Pkg\.Revision\s*=\s*27\.2\.12479018\s*$' -or
    $NdkProperties -notmatch '(?m)^Pkg\.ReleaseName\s*=\s*r27c\s*$') {
    throw ("NDK directory is not the required r27c build: {0}" -f $ResolvedNdkRoot)
}

$ResolvedJavaHome = Resolve-JavaHome $JavaHome $ResolvedEngineRoot
$BuildToolsRoot = Find-AndroidBuildTools $ResolvedSdkRoot
$Aapt2 = Join-Path $BuildToolsRoot "aapt2.exe"
$ApkSigner = Join-Path $BuildToolsRoot "apksigner.bat"
$ReadElf = Join-Path $ResolvedNdkRoot "toolchains\llvm\prebuilt\windows-x86_64\bin\llvm-readelf.exe"
Assert-File $ReadElf "NDK llvm-readelf"
$TarCommand = Get-Command "tar.exe" -ErrorAction Stop

$AndroidSection = "/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"
$ExpectedPackageName = Get-IniValue $EngineConfig $AndroidSection "PackageName"
$ExpectedVersionCode = Get-IniValue $EngineConfig $AndroidSection "StoreVersion"
$ExpectedVersionName = Get-IniValue $EngineConfig $AndroidSection "VersionDisplayName"
$PackageInsideApk = Get-IniValue $EngineConfig $AndroidSection "bPackageDataInsideApk"
$BuildArm64 = Get-IniValue $EngineConfig $AndroidSection "bBuildForArm64"
$BuildX64 = Get-IniValue $EngineConfig $AndroidSection "bBuildForX8664"
if (-not $PackageInsideApk.Equals("True", [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "bPackageDataInsideApk must be True for the single-APK workflow."
}
if (-not $BuildArm64.Equals("True", [System.StringComparison]::OrdinalIgnoreCase) -or
    -not $BuildX64.Equals("False", [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "The single-APK workflow requires ARM64=True and X8664=False."
}

$AndroidPluginText = Get-Content -LiteralPath $AndroidPlugin -Raw
if ($AndroidPluginText -notmatch 'Build/Android/LiteRtLmModelChunks' -or
    $AndroidPluginText -notmatch 'assets/litertlm') {
    throw "LiteRTLMUnreal_APL.xml is not wired to copy model chunks into APK assets/litertlm."
}

$ModelItem = Get-Item -LiteralPath $ModelPath
if ($ModelItem.Length -lt 1) {
    throw ("Model is empty: {0}" -f $ModelPath)
}

if ([string]::IsNullOrWhiteSpace($ArchiveDirectory)) {
    $ArchiveDirectory = Join-Path $ProjectRoot "Packaged\AndroidV6_SingleAPK"
}
elseif (-not [System.IO.Path]::IsPathRooted($ArchiveDirectory)) {
    $ArchiveDirectory = Join-Path $ProjectRoot $ArchiveDirectory
}
$ArchiveDirectory = [System.IO.Path]::GetFullPath($ArchiveDirectory)
$PackagedRoot = Join-Path $ProjectRoot "Packaged"
if (-not (Test-IsStrictChildPath $PackagedRoot $ArchiveDirectory)) {
    throw ("ArchiveDirectory must be a child of {0}; refusing unsafe cleanup target {1}." -f $PackagedRoot, $ArchiveDirectory)
}

Write-Host ("Project:       " + $ProjectFile)
Write-Host ("Engine:        " + $ResolvedEngineRoot)
Write-Host ("Android SDK:   " + $ResolvedSdkRoot)
Write-Host ("Android NDK:   " + $ResolvedNdkRoot + " (r27c)")
Write-Host ("JDK:           " + $ResolvedJavaHome)
Write-Host ("Build tools:   " + $BuildToolsRoot)
Write-Host ("Model:         {0} ({1:N0} bytes)" -f $ModelPath, $ModelItem.Length)
Write-Host ("Package:       {0}, versionCode={1}, versionName={2}" -f $ExpectedPackageName, $ExpectedVersionCode, $ExpectedVersionName)
Write-Host ("Archive:       " + $ArchiveDirectory)

$EnvironmentNames = @("ANDROID_HOME", "ANDROID_SDK_ROOT", "NDKROOT", "NDK_ROOT", "JAVA_HOME")
$OriginalEnvironment = @{}
foreach ($Name in $EnvironmentNames) {
    $OriginalEnvironment[$Name] = [Environment]::GetEnvironmentVariable($Name, "Process")
}

try {
    # Process-only overrides. They are restored in finally; this script never calls setx.
    [Environment]::SetEnvironmentVariable("ANDROID_HOME", $ResolvedSdkRoot, "Process")
    [Environment]::SetEnvironmentVariable("ANDROID_SDK_ROOT", $ResolvedSdkRoot, "Process")
    [Environment]::SetEnvironmentVariable("NDKROOT", $ResolvedNdkRoot, "Process")
    [Environment]::SetEnvironmentVariable("NDK_ROOT", $ResolvedNdkRoot, "Process")
    [Environment]::SetEnvironmentVariable("JAVA_HOME", $ResolvedJavaHome, "Process")

    Write-Step "Generate lossless model chunks for the Blueprint-only project"
    Write-ModelChunks $ModelPath $ChunkDirectory $ModelFileName $ModelChunkBytes

    Write-Step "Validate generated model chunks before packaging"
    $ChunkLayout = Get-ChunkLayout $ChunkManifestPath $ChunkDirectory $ModelPath $ModelFileName
    Write-Host ("Chunks: {0}; byte sum: {1:N0}; manifest bytes: {2:N0}" -f $ChunkLayout.Count, $ChunkLayout.TotalBytes, $ChunkLayout.ManifestBytes)

    Write-Step "Clean only approved Android outputs inside this workspace"
    $CookDirectory = Join-Path $ProjectRoot "Saved\Cooked\Android_ASTC"
    $StageDirectory = Join-Path $ProjectRoot "Saved\StagedBuilds\Android_ASTC"
    $GradleAssetsDirectory = Join-Path $ProjectRoot "Intermediate\Android"
    $NativeAuditDirectory = Join-Path $ProjectRoot "Saved\AndroidNativeAudit"
    $ApprovedCleanPaths = @($CookDirectory, $StageDirectory, $GradleAssetsDirectory, $NativeAuditDirectory, $ArchiveDirectory)
    foreach ($Path in $ApprovedCleanPaths) {
        Remove-ApprovedWorkspaceDirectory $Path $ApprovedCleanPaths
    }
    New-Item -ItemType Directory -Path $ArchiveDirectory -Force | Out-Null

    Write-Step "Run UAT Cook / Stage / Pak / Package / Archive for Android ASTC"
    $RunUat = Join-Path $ResolvedEngineRoot "Engine\Build\BatchFiles\RunUAT.bat"
    Invoke-NativeCommand $RunUat @(
        "BuildCookRun",
        ("-project=" + $ProjectFile),
        "-noP4",
        "-platform=Android",
        "-cookflavor=ASTC",
        ("-clientconfig=" + $Configuration),
        "-build",
        "-skipbuildeditor",
        "-cook",
        "-stage",
        "-pak",
        "-package",
        "-archive",
        ("-archivedirectory=" + $ArchiveDirectory),
        "-prereqs",
        "-nodebuginfo",
        "-unattended",
        "-utf8output"
    )

    Write-Step "Verify single APK, package metadata, v2 signature, and embedded model assets"
    $Apks = @(Get-ChildItem -LiteralPath $ArchiveDirectory -Recurse -File | Where-Object { $_.Extension.Equals(".apk", [System.StringComparison]::OrdinalIgnoreCase) })
    if ($Apks.Count -ne 1) {
        throw ("Expected exactly one APK in archive, found {0}." -f $Apks.Count)
    }
    $ExternalObbs = @(Get-ChildItem -LiteralPath $ArchiveDirectory -Recurse -File | Where-Object { $_.Extension.Equals(".obb", [System.StringComparison]::OrdinalIgnoreCase) })
    if ($ExternalObbs.Count -ne 0) {
        throw ("Expected no external OBB, found {0}." -f $ExternalObbs.Count)
    }

    $Apk = $Apks[0]
    if ($Apk.Length -le $ModelItem.Length) {
        throw ("APK is unexpectedly smaller than the embedded model: {0:N0} bytes." -f $Apk.Length)
    }

    $BadgingOutput = Invoke-NativeCommandCapture $Aapt2 @("dump", "badging", $Apk.FullName)
    $PackageLine = @($BadgingOutput | Where-Object { $_ -match '^package:' })
    if ($PackageLine.Count -ne 1) {
        throw "aapt2 did not return exactly one package line."
    }
    $PackageMatch = [regex]::Match($PackageLine[0], "name='(?<name>[^']+)'\s+versionCode='(?<code>[^']+)'\s+versionName='(?<version>[^']*)'")
    if (-not $PackageMatch.Success) {
        throw ("Could not parse aapt2 package metadata: {0}" -f $PackageLine[0])
    }
    if (-not $PackageMatch.Groups["name"].Value.Equals($ExpectedPackageName, [System.StringComparison]::Ordinal) -or
        -not $PackageMatch.Groups["code"].Value.Equals($ExpectedVersionCode, [System.StringComparison]::Ordinal) -or
        -not $PackageMatch.Groups["version"].Value.Equals($ExpectedVersionName, [System.StringComparison]::Ordinal)) {
        throw ("APK metadata mismatch. Actual: {0}" -f $PackageLine[0])
    }

    $SignerOutput = Invoke-NativeCommandCapture $ApkSigner @("verify", "--verbose", "--print-certs", $Apk.FullName)
    if (-not ($SignerOutput -match '^Verified using v2 scheme \(APK Signature Scheme v2\): true$')) {
        throw "apksigner verification passed, but APK Signature Scheme v2 is not enabled."
    }

    $TarOutput = Invoke-NativeCommandCapture $TarCommand.Source @("-tf", $Apk.FullName)
    $TarEntries = @($TarOutput | ForEach-Object { Normalize-ArchiveEntryName $_ } | Where-Object { $_.Length -gt 0 })
    $ExpectedAssetEntries = New-Object System.Collections.Generic.List[string]
    $ExpectedAssetEntries.Add("assets/main.obb.png")
    $ExpectedAssetEntries.Add("assets/litertlm/" + $ModelFileName + ".parts")
    foreach ($ChunkFile in $ChunkLayout.Files) {
        $ExpectedAssetEntries.Add("assets/litertlm/" + $ChunkFile.Name)
    }

    foreach ($EntryName in $ExpectedAssetEntries) {
        if (@($TarEntries | Where-Object { $_.Equals($EntryName, [System.StringComparison]::Ordinal) }).Count -ne 1) {
            throw ("tar listing does not contain exactly one required APK entry: {0}" -f $EntryName)
        }
    }
    $ActualLiteRtEntries = @($TarEntries | Where-Object { $_.StartsWith("assets/litertlm/", [System.StringComparison]::Ordinal) })
    $ExpectedLiteRtEntries = @($ExpectedAssetEntries | Where-Object { $_.StartsWith("assets/litertlm/", [System.StringComparison]::Ordinal) })
    $AssetDifference = @(Compare-Object -ReferenceObject $ExpectedLiteRtEntries -DifferenceObject $ActualLiteRtEntries)
    if ($AssetDifference.Count -ne 0) {
        throw ("Unexpected assets/litertlm entries in APK:`n" + (($AssetDifference | Out-String).Trim()))
    }

    Write-Host "Required tar entries:"
    foreach ($EntryName in $ExpectedAssetEntries) {
        Write-Host ("  " + $EntryName)
    }

    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $Zip = [System.IO.Compression.ZipFile]::OpenRead($Apk.FullName)
    try {
        $MainPayloadEntry = $Zip.GetEntry("assets/main.obb.png")
        if ($null -eq $MainPayloadEntry -or $MainPayloadEntry.Length -lt 1) {
            throw "Embedded assets/main.obb.png is missing or empty."
        }

        $EmbeddedManifestEntry = $Zip.GetEntry("assets/litertlm/" + $ModelFileName + ".parts")
        if ($null -eq $EmbeddedManifestEntry) {
            throw "Embedded LiteRT-LM manifest is missing."
        }
        $Reader = New-Object System.IO.StreamReader($EmbeddedManifestEntry.Open(), [System.Text.Encoding]::UTF8, $true)
        try {
            $EmbeddedManifestText = $Reader.ReadToEnd().Replace("`r`n", "`n")
        }
        finally {
            $Reader.Dispose()
        }
        if (-not $EmbeddedManifestText.Equals($ChunkLayout.ManifestText, [System.StringComparison]::Ordinal)) {
            throw "Embedded model manifest does not match the manifest produced by Android Build."
        }

        [long]$EmbeddedChunkBytes = 0
        foreach ($ChunkFile in $ChunkLayout.Files) {
            $EntryName = "assets/litertlm/" + $ChunkFile.Name
            $Entry = $Zip.GetEntry($EntryName)
            if ($null -eq $Entry) {
                throw ("Embedded model chunk is missing: {0}" -f $EntryName)
            }
            if ($Entry.Length -ne $ChunkFile.Length) {
                throw ("Embedded chunk length mismatch for {0}: APK={1}, source chunk={2}." -f $EntryName, $Entry.Length, $ChunkFile.Length)
            }
            $EmbeddedChunkBytes += $Entry.Length
        }

        if ($EmbeddedChunkBytes -ne $ModelItem.Length -or
            $EmbeddedChunkBytes -ne $ChunkLayout.ManifestBytes) {
            throw ("Embedded chunk byte sum ({0}) does not match model/manifest ({1})." -f $EmbeddedChunkBytes, $ModelItem.Length)
        }

        # Validate the libraries that actually reached the signed APK, rather
        # than trusting source-tree filenames. The stable wrapper must export
        # LiteRtLm_GetApi, and its required LiteRtCreateModelFromFd import must
        # be provided by the packaged libLiteRt.so.
        New-Item -ItemType Directory -Path $NativeAuditDirectory -Force | Out-Null
        $NativeEntries = @{
            "liblitert_lm_wrapper.so" = "lib/arm64-v8a/liblitert_lm_wrapper.so"
            "libLiteRt.so" = "lib/arm64-v8a/libLiteRt.so"
        }
        foreach ($NativeName in $NativeEntries.Keys) {
            $EntryName = $NativeEntries[$NativeName]
            $NativeEntry = $Zip.GetEntry($EntryName)
            if ($null -eq $NativeEntry -or $NativeEntry.Length -lt 1) {
                throw ("Required Android native library is missing from APK: {0}" -f $EntryName)
            }

            $Destination = Join-Path $NativeAuditDirectory $NativeName
            $InputStream = $NativeEntry.Open()
            $OutputStream = [System.IO.File]::Open(
                $Destination,
                [System.IO.FileMode]::Create,
                [System.IO.FileAccess]::Write,
                [System.IO.FileShare]::None)
            try {
                $InputStream.CopyTo($OutputStream)
            }
            finally {
                $OutputStream.Dispose()
                $InputStream.Dispose()
            }
        }

        $WrapperSymbols = Invoke-NativeCommandCapture $ReadElf @(
            "--dyn-syms", "--wide", (Join-Path $NativeAuditDirectory "liblitert_lm_wrapper.so"))
        $CoreSymbols = Invoke-NativeCommandCapture $ReadElf @(
            "--dyn-syms", "--wide", (Join-Path $NativeAuditDirectory "libLiteRt.so"))
        if (-not ($WrapperSymbols -match '\bGLOBAL\s+DEFAULT\s+\S+\s+LiteRtLm_GetApi@@LITERT_LM_WRAPPER_1\.0$')) {
            throw "Packaged Android wrapper does not export stable ABI LiteRtLm_GetApi."
        }
        if (-not ($WrapperSymbols -match '\bUND\s+LiteRtCreateModelFromFd@VERS_1\.0$')) {
            throw "Packaged Android wrapper does not declare the expected LiteRtCreateModelFromFd dependency."
        }
        if (-not ($CoreSymbols -match '\bGLOBAL\s+DEFAULT\s+\S+\s+LiteRtCreateModelFromFd@@VERS_1\.0$')) {
            throw "Packaged Android libLiteRt.so does not provide LiteRtCreateModelFromFd required by the wrapper."
        }
    }
    finally {
        $Zip.Dispose()
    }

    Write-Host ""
    Write-Host "Android single-APK package verified successfully." -ForegroundColor Green
    Write-Host ("APK:             " + $Apk.FullName)
    Write-Host ("APK bytes:       {0:N0}" -f $Apk.Length)
    Write-Host ("External OBB:    0")
    Write-Host ("Package/version: {0} / {1} / {2}" -f $ExpectedPackageName, $ExpectedVersionCode, $ExpectedVersionName)
    Write-Host ("Signature:       APK Signature Scheme v2 = true")
    Write-Host "Native ABI:       stable wrapper/core symbol contract verified inside APK"
    Write-Host ("Model chunks:    {0}, {1:N0} bytes (matches source and manifest)" -f $ChunkLayout.Count, $ChunkLayout.TotalBytes)
}
finally {
    foreach ($Name in $EnvironmentNames) {
        [Environment]::SetEnvironmentVariable($Name, $OriginalEnvironment[$Name], "Process")
    }
}
