#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$Version = '3.25a',
    [string]$ExpectedSha256 = '',
    [switch]$Force,
    # Skip building DxLib from its official source (needed for model loading in every configuration).
    [switch]$SkipSourceBuild
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$ThirdParty = Join-Path $Root 'ThirdParty'
$Destination = Join-Path $ThirdParty ('DxLib-' + $Version)
$Manifest = Join-Path $ThirdParty 'dxlib-sdk.json'
if ($Version -notmatch '^\d+\.\d+[a-z]?$') { throw 'Invalid DxLib version.' }
if ($ExpectedSha256 -and $ExpectedSha256 -notmatch '^[0-9a-fA-F]{64}$') { throw 'ExpectedSha256 must contain 64 hex characters.' }
$UseExisting = $false
if ((Test-Path $Manifest) -and -not $Force) {
    $Existing = Get-Content -Raw $Manifest | ConvertFrom-Json
    if ($Existing.version -eq $Version -and (Test-Path (Join-Path $Existing.include_directory 'DxLib.h'))) {
        if ($ExpectedSha256 -and $Existing.sha256 -ne $ExpectedSha256.ToLowerInvariant()) { throw 'Cached SDK hash differs from ExpectedSha256. Use -Force to download again.' }
        Write-Host ('Using existing SDK: ' + $Existing.include_directory)
        $UseExisting = $true
    }
}
if (-not $UseExisting) {
New-Item -ItemType Directory -Force -Path $ThirdParty | Out-Null
$ArchiveName = 'DxLib_VC' + $Version.Replace('.', '_') + '.zip'
$Url = 'https://dxlib.xsrv.jp/DxLib/' + $ArchiveName
$Archive = Join-Path $ThirdParty $ArchiveName
$Partial = $Archive + '.partial'
$Stage = Join-Path $ThirdParty ('extract-' + [guid]::NewGuid().ToString('N'))
try {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Write-Host ('Downloading the official SDK: ' + $Url)
    $ProgressPreference = 'SilentlyContinue'
    Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $Partial -TimeoutSec 600
    $Hash = (Get-FileHash -LiteralPath $Partial -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($ExpectedSha256 -and $Hash -ne $ExpectedSha256.ToLowerInvariant()) { throw 'SDK SHA-256 mismatch.' }
    if ((Get-Item $Partial).Length -lt 1024) { throw 'SDK download is unexpectedly small.' }
    Move-Item -LiteralPath $Partial -Destination $Archive -Force
    Expand-Archive -LiteralPath $Archive -DestinationPath $Stage
    $Header = Get-ChildItem -LiteralPath $Stage -Filter DxLib.h -File -Recurse |
        Where-Object { @(Get-ChildItem -LiteralPath $_.DirectoryName -Filter '*.lib' -File).Count -gt 0 } |
        Select-Object -First 1
    if ($null -eq $Header) { throw 'No DxLib.h with VC libraries found in the official archive.' }
    $Relative = $Header.DirectoryName.Substring($Stage.Length).TrimStart('\', '/')
    if (Test-Path $Destination) {
        if (-not $Force) { throw ('Destination exists; use -Force explicitly: ' + $Destination) }
        Remove-Item -LiteralPath $Destination -Recurse -Force
    }
    Move-Item -LiteralPath $Stage -Destination $Destination
    $Include = Join-Path $Destination $Relative
    [ordered]@{
        version = $Version
        url = $Url
        sha256 = $Hash
        hash_verified_against_expected = [bool]$ExpectedSha256
        include_directory = $Include
        downloaded_at_utc = [DateTime]::UtcNow.ToString('o')
    } | ConvertTo-Json | Set-Content -LiteralPath $Manifest -Encoding UTF8
    Write-Host ('SDK installed: ' + $Include)
    Write-Host ('SHA-256 recorded: ' + $Hash)
    if (-not $ExpectedSha256) {
        Write-Warning 'No expected digest was supplied. The recorded hash is a download record, not independent authenticity verification.'
    }
} finally {
    if (Test-Path $Partial) { Remove-Item -LiteralPath $Partial -Force }
    if (Test-Path $Stage) { Remove-Item -LiteralPath $Stage -Recurse -Force }
}
}

# DxLib built from its official source without the Autodesk FBX SDK.
# The official DxLib_vs2015_x64_MT.lib links its MV1 loader against the FBX SDK, so Release builds cannot use
# models with it. The framework converts .fbx with ufbx, so this build needs no FBX SDK and no license agreement
# beyond DxLib's own. CMake/FindDxLib.cmake picks up ThirdParty/DxLib-<version>-source automatically.
if (-not $SkipSourceBuild) {
    $KnownSourceHashes = @{ '3.25a' = '2f09078692d3b64448c6ffe80392d77d0f413ce652115a1d7f0063baf475322a' }
    $SourceName = 'DxLibMake' + $Version.Replace('.', '_') + '.zip'
    $SourceZip = Join-Path $ThirdParty $SourceName
    $SourceOut = Join-Path $ThirdParty ('DxLib-' + $Version + '-source')
    $ExpectedSource = if ($KnownSourceHashes.ContainsKey($Version)) { $KnownSourceHashes[$Version] } else { '' }
    if ((Test-Path (Join-Path $SourceOut 'DxLibFbx.json')) -and -not $Force) {
        Write-Host ('Using existing DxLib source build: ' + $SourceOut)
    } else {
        if (-not (Test-Path $SourceZip)) {
            $SourceUrl = 'https://dxlib.xsrv.jp/DxLib/' + $SourceName
            $SourcePartial = $SourceZip + '.partial'
            try {
                [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
                Write-Host ('Downloading the official DxLib source: ' + $SourceUrl)
                $ProgressPreference = 'SilentlyContinue'
                Invoke-WebRequest -UseBasicParsing -Uri $SourceUrl -OutFile $SourcePartial -TimeoutSec 600
                $SourceHash = (Get-FileHash -LiteralPath $SourcePartial -Algorithm SHA256).Hash.ToLowerInvariant()
                if ($ExpectedSource -and $SourceHash -ne $ExpectedSource) { throw ('DxLib source SHA-256 mismatch: ' + $SourceHash) }
                Move-Item -LiteralPath $SourcePartial -Destination $SourceZip -Force
            } finally {
                if (Test-Path $SourcePartial) { Remove-Item -LiteralPath $SourcePartial -Force }
            }
        }
        if (-not $ExpectedSource) {
            Write-Warning ('No pinned SHA-256 for the DxLib ' + $Version + ' source; the archive is used without independent verification.')
        }
        Write-Host 'Building DxLib from source without the FBX SDK (Debug and Release, several minutes)...'
        & (Join-Path $PSScriptRoot 'DxLibFbx/BuildDxLibFbx.ps1') -SourceZip $SourceZip -ExpectedSourceSha256 $ExpectedSource -OutDir $SourceOut
    }
}
