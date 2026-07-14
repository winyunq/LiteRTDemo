[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("Android", "Windows")]
    [string]$Target,

    [switch]$Download,

    [string]$OutputDirectory = $PSScriptRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$ReleaseBaseUrl = "https://github.com/winyunq/LiteRTDemo/releases/download/v5.0.0"
$ManifestName = if ($Target -eq "Android") {
    "LiteRTDemo-Android-Shipping-arm64.apk.manifest.json"
}
else {
    "LiteRTDemo-Windows-v5.0.0.zip.manifest.json"
}

$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$ManifestPath = Join-Path $OutputDirectory $ManifestName

function Receive-ReleaseAsset {
    param(
        [string]$Name,
        [string]$Destination
    )

    $Uri = "$ReleaseBaseUrl/$Name"
    Write-Host "Downloading $Name"
    Invoke-WebRequest -UseBasicParsing -Uri $Uri -OutFile $Destination
}

if ($Download) {
    Receive-ReleaseAsset $ManifestName $ManifestPath
}
elseif (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
    throw "Manifest not found: $ManifestPath. Download all parts and the manifest, or add -Download."
}

$Manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
if ($Manifest.schema -ne 1 -or [string]::IsNullOrWhiteSpace($Manifest.artifact)) {
    throw "Unsupported release manifest: $ManifestPath"
}

foreach ($Part in $Manifest.parts) {
    $PartPath = Join-Path $OutputDirectory $Part.name
    if ($Download -and -not (Test-Path -LiteralPath $PartPath -PathType Leaf)) {
        Receive-ReleaseAsset $Part.name $PartPath
    }
    if (-not (Test-Path -LiteralPath $PartPath -PathType Leaf)) {
        throw "Missing release part: $PartPath"
    }

    $PartItem = Get-Item -LiteralPath $PartPath
    if ($PartItem.Length -ne [long]$Part.bytes) {
        throw "Part length mismatch: $($Part.name)"
    }
    $PartHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $PartPath).Hash
    if (-not $PartHash.Equals([string]$Part.sha256, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Part SHA-256 mismatch: $($Part.name)"
    }
    Write-Host "Verified $($Part.name)"
}

$ArtifactPath = Join-Path $OutputDirectory $Manifest.artifact
if (Test-Path -LiteralPath $ArtifactPath) {
    Remove-Item -LiteralPath $ArtifactPath -Force
}

$OutputStream = [System.IO.File]::Open(
    $ArtifactPath,
    [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write,
    [System.IO.FileShare]::None)
try {
    foreach ($Part in $Manifest.parts) {
        $PartPath = Join-Path $OutputDirectory $Part.name
        $InputStream = [System.IO.File]::OpenRead($PartPath)
        try {
            $InputStream.CopyTo($OutputStream, 8MB)
        }
        finally {
            $InputStream.Dispose()
        }
    }
}
finally {
    $OutputStream.Dispose()
}

$ArtifactItem = Get-Item -LiteralPath $ArtifactPath
if ($ArtifactItem.Length -ne [long]$Manifest.bytes) {
    throw "Assembled artifact length mismatch: $ArtifactPath"
}
$ArtifactHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $ArtifactPath).Hash
if (-not $ArtifactHash.Equals([string]$Manifest.sha256, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Assembled artifact SHA-256 mismatch: $ArtifactPath"
}

Write-Host ""
Write-Host "LiteRTDemo $Target artifact assembled and verified." -ForegroundColor Green
Write-Host "Output: $ArtifactPath"
Write-Host "SHA-256: $ArtifactHash"
if ($Target -eq "Android") {
    Write-Host "Copy the APK to an ARM64 Android device and install it."
}
else {
    Write-Host "Extract the ZIP and run LiteRTDemo.exe on Windows 11."
}
