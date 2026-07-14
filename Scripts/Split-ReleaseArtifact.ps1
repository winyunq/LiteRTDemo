[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$InputFile,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,

    [long]$PartBytes = 1900000000
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

if ($PartBytes -lt 1MB -or $PartBytes -ge 2GB) {
    throw "PartBytes must be at least 1 MiB and strictly below GitHub's 2 GiB per-file limit."
}

$InputFile = [System.IO.Path]::GetFullPath($InputFile)
$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
if (-not (Test-Path -LiteralPath $InputFile -PathType Leaf)) {
    throw "Input file not found: $InputFile"
}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

$InputItem = Get-Item -LiteralPath $InputFile
$ArtifactName = $InputItem.Name
$ManifestPath = Join-Path $OutputDirectory ($ArtifactName + ".manifest.json")

Get-ChildItem -LiteralPath $OutputDirectory -File |
    Where-Object { $_.Name -match ('^' + [regex]::Escape($ArtifactName) + '\.part\d+$') } |
    Remove-Item -Force
if (Test-Path -LiteralPath $ManifestPath) {
    Remove-Item -LiteralPath $ManifestPath -Force
}

$Parts = New-Object System.Collections.Generic.List[object]
$Buffer = New-Object byte[] (8MB)
$InputStream = [System.IO.File]::Open(
    $InputFile,
    [System.IO.FileMode]::Open,
    [System.IO.FileAccess]::Read,
    [System.IO.FileShare]::Read)
try {
    $PartIndex = 1
    while ($InputStream.Position -lt $InputStream.Length) {
        $PartName = "{0}.part{1:D2}" -f $ArtifactName, $PartIndex
        $PartPath = Join-Path $OutputDirectory $PartName
        $PartStream = [System.IO.File]::Open(
            $PartPath,
            [System.IO.FileMode]::Create,
            [System.IO.FileAccess]::Write,
            [System.IO.FileShare]::None)
        try {
            $Remaining = [Math]::Min($PartBytes, $InputStream.Length - $InputStream.Position)
            while ($Remaining -gt 0) {
                $ReadLength = [int][Math]::Min([long]$Buffer.Length, $Remaining)
                $Read = $InputStream.Read($Buffer, 0, $ReadLength)
                if ($Read -le 0) {
                    throw "Unexpected end of input while writing $PartName"
                }
                $PartStream.Write($Buffer, 0, $Read)
                $Remaining -= $Read
            }
        }
        finally {
            $PartStream.Dispose()
        }

        $PartItem = Get-Item -LiteralPath $PartPath
        $PartHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $PartPath).Hash
        $Parts.Add([ordered]@{
            name = $PartName
            bytes = $PartItem.Length
            sha256 = $PartHash
        })
        Write-Host ("Created {0} ({1:N0} bytes)" -f $PartName, $PartItem.Length)
        $PartIndex++
    }
}
finally {
    $InputStream.Dispose()
}

$ArtifactHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $InputFile).Hash
$Manifest = [ordered]@{
    schema = 1
    artifact = $ArtifactName
    bytes = $InputItem.Length
    sha256 = $ArtifactHash
    part_bytes = $PartBytes
    parts = $Parts
}
$Manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $ManifestPath -Encoding utf8

Write-Host ""
Write-Host "Release artifact split successfully." -ForegroundColor Green
Write-Host "Manifest: $ManifestPath"
Write-Host "Artifact SHA-256: $ArtifactHash"
